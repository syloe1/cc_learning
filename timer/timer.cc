
/*
双链表 O (1) 删除。资源只有一个 user_data 上下文
*/
#include <ctime>
#include <functional>
#include <mutex>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

class TimerNode;
struct client_data {
    sockaddr_in address;
    int sockfd;
    TimerNode *timer;
};

class TimerNode {
    // 单个定时器节点， 保存： 到期时间， 回调， 业务数据， 前后指针
public:
    using TimerCallback = std::function<void(client_data *)>;
    std::time_t expire; // 到期时间
    TimerCallback cb;
    client_data *user_data;
    TimerNode *prev;
    TimerNode *next;
    TimerNode()
        : expire(0), cb(), user_data(nullptr), prev(nullptr), next(nullptr) {}
};

// 注意：TimerListMgr 接管 TimerNode 的所有权
// （del_timer / tick / 析构函数都会 delete 它），
// 所以 TimerNode 必须用 new 分配，不能放在栈上。
class TimerListMgr {
public:
    TimerListMgr();
    ~TimerListMgr();
    void add_timer(TimerNode *timer);
    void adjust_timer(TimerNode *timer);
    void del_timer(TimerNode *timer);
    // tick () 是【被外部主动调用】的，和 Redis
    // 的定时任务（serverCron）思路几乎一模一样。
    void tick();

private:
    TimerNode *head;
    TimerNode *tail;
    std::mutex mtx_;
    void add_timer(TimerNode *timer, TimerNode *list_head);
    // 禁止拷贝
    TimerListMgr(const TimerListMgr &) = delete;
    TimerListMgr &operator=(const TimerListMgr &) = delete;
};

TimerListMgr::TimerListMgr() : head(nullptr), tail(nullptr) {}

TimerListMgr::~TimerListMgr() {
    std::lock_guard<std::mutex> lk(mtx_);
    TimerNode *tmp = head;
    while (tmp) {
        auto *del = tmp;
        tmp = tmp->next;
        // 断开业务侧的回指，否则业务数据里留着已释放的节点
        if (del->user_data) {
            del->user_data->timer = nullptr;
        }
        delete del;
    }
    head = nullptr;
    tail = nullptr;
}

void TimerListMgr::add_timer(TimerNode *timer) {
    if (!timer) {
        return;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    // < head
    if (!head) {
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    // > head 递归插入
    add_timer(timer, head);
}

void TimerListMgr::del_timer(TimerNode *timer) {
    if (!timer) {
        return;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    // 统一处理 单节点 / head / tail / 中间 四种情况。
    // head==timer 的判断同时让「已经摘除的节点」再删一次也不会破坏链表。
    if (timer->prev) {
        timer->prev->next = timer->next;
    } else if (head == timer) {
        head = timer->next;
    }

    if (timer->next) {
        timer->next->prev = timer->prev;
    } else if (tail == timer) {
        tail = timer->prev;
    }

    timer->prev = timer->next = nullptr;
    if (timer->user_data) {
        timer->user_data->timer = nullptr;
    }
    delete timer;
}

void TimerListMgr::tick() {
    const std::time_t cur = std::time(nullptr);
    std::vector<TimerNode *> expired;

    {
        std::lock_guard<std::mutex> lk(mtx_);
        while (head && head->expire <= cur) {
            TimerNode *del = head;
            head = head->next;
            if (head) {
                head->prev = nullptr;
            } else {
                tail = nullptr;
            }
            // 节点已经摘除，业务侧的 timer 立刻失效，
            // 这样回调里再调 del_timer 也是安全的
            del->prev = del->next = nullptr;
            if (del->user_data) {
                del->user_data->timer = nullptr;
            }
            expired.push_back(del);
        }
    }

    // 回调在锁外执行：回调里通常会关连接 / 调 del_timer，
    // 持锁调用会自己把自己锁死（std::mutex 不可重入）
    for (TimerNode *del : expired) {
        if (del->cb && del->user_data) {
            del->cb(del->user_data);
        }
        delete del;
    }
}

void TimerListMgr::add_timer(TimerNode *timer, TimerNode *list_head) {
    if (!timer || !list_head) {
        return;
    }
    TimerNode *prev = list_head;
    TimerNode *tmp = prev->next;
    while (tmp) {
        if (timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            return;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    // 比所有节点都大，挂到尾部
    prev->next = timer;
    timer->prev = prev;
    timer->next = nullptr;
    tail = timer;
}

void TimerListMgr::adjust_timer(TimerNode *timer) {
    // 空指针保护
    if (!timer) {
        return;
    }
    // 加锁，并发安全
    std::lock_guard<std::mutex> lk(mtx_);

    // 前驱和后继都还有序，位置没变，不用动。
    // 只比后继会在 expire 变小（比如重新设置更近的到期时间）时漏掉
    // 需要往前挪的情况，所以两边一起看。
    const bool after_ok = !timer->next || timer->expire <= timer->next->expire;
    const bool before_ok = !timer->prev || timer->prev->expire <= timer->expire;
    if (after_ok && before_ok) {
        return;
    }

    // 需要挪位置：先从链表摘除
    if (timer->prev) {
        timer->prev->next = timer->next;
    } else if (head == timer) {
        head = timer->next;
    }

    if (timer->next) {
        timer->next->prev = timer->prev;
    } else if (tail == timer) {
        tail = timer->prev;
    }

    // 摘除之后，清空 timer 自身的链表指针，脱离链表
    timer->prev = timer->next = nullptr;

    // 重新插入链表
    if (!head) {
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

