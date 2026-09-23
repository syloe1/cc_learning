// TimerListMgr 使用演示：验证有序双链表的 插入 / 触发 / 删除 / 重排。
//
// 说明：timer.cc 是单文件写法的练习（类声明和实现都写在里面，没有拆出头文件），
// 所以这里直接把 .cc include 进来，保持它原样不动。
// 项目以后要是变大，应该拆成 timer.h（声明）+ timer.cc（实现），由 Makefile 分别编译。
//
// 所有权约定（重要）：
//   TimerListMgr 接管 TimerNode 的所有权——del_timer / tick / 析构都会 delete 节点。
//   所以节点必须 new 出来，而且**一旦交出去就不能再碰这个指针**。
//   client_data（业务上下文）不归 TimerListMgr 管，由业务侧自己释放。

#include <chrono>
#include <cstdio>
#include <ctime>
#include <thread>

#include "timer.cc"

static int fired_count = 0;

// 业务回调：这里模拟「连接空闲超时，该关掉了」
static void on_expire(client_data *d) {
    ++fired_count;
    std::printf("     -> 到期回调: sockfd=%d\n", d->sockfd);
    delete d; // client_data 由业务侧负责回收
}

// 造一个定时器：节点和业务上下文都 new 出来，节点交给 mgr 托管
static TimerNode *add_timer(TimerListMgr &mgr, int fd, long long delay_sec) {
    auto *d = new client_data();
    d->sockfd = fd;
    d->address = sockaddr_in{};
    d->timer = new TimerNode();
    d->timer->expire = std::time(nullptr) + delay_sec;
    d->timer->cb = on_expire;
    d->timer->user_data = d;
    mgr.add_timer(d->timer);
    return d->timer;
}

int main() {
    TimerListMgr mgr;
    std::printf("起始时间戳: %lld\n", (long long)std::time(nullptr));

    std::printf("\n[1] 乱序加入 3 个定时器：+2s / +1s / +60s\n");
    add_timer(mgr, 2002, 2);
    add_timer(mgr, 2001, 1);
    TimerNode *far = add_timer(mgr, 2003, 60); // 留到 [7] 手动清理

    std::printf("\n[2] 立刻 tick()，应该谁都没到期\n");
    mgr.tick();
    std::printf("     累计触发 %d 个（期望 0）\n", fired_count);

    std::printf("\n[3] 睡 3 秒...\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::printf("\n[4] tick()：2001(+1s) 和 2002(+2s) 应该都到期\n");
    mgr.tick();
    std::printf("     累计触发 %d 个（期望 2）\n", fired_count);

    std::printf("\n[5] 演示 del_timer 与 adjust_timer：\n");
    TimerNode *mid = add_timer(mgr, 2003, 60);
    TimerNode *long_one = add_timer(mgr, 2004, 50);
    TimerNode *soonest = add_timer(mgr, 2005, 100);
    std::printf("     已按乱序加入，链表自动排序为：\n");
    std::printf("       2004(+50s) -> 2003(+60s) -> 2005(+100s)\n");

    // 删中间那个。del_timer 只接管节点，业务数据得自己释放，
    // 而且要先把 user_data 取出来——del_timer 返回后节点就是野指针了。
    client_data *mid_data = mid->user_data;
    mgr.del_timer(mid);
    delete mid_data;
    std::printf("     删掉中间的 2003 后：2004(+50s) -> 2005(+100s)\n");

    soonest->expire = std::time(nullptr); // 把 2005 提前到「现在」
    mgr.adjust_timer(soonest);            // 应该被挪到链表头部
    std::printf("     已把 2005 提前并重排，tick() 应该只有它触发\n");
    mgr.tick();
    std::printf("     累计触发 %d 个（期望 3）\n", fired_count);
    // 注意：soonest 已经被 tick() 回收了，这里绝对不能再碰它。
    // （上面 on_expire 里已经连 client_data 一起 delete 掉了）

    std::printf("\n[6] 再 tick() 一次：只剩 +50s 的没到期，应该什么也不做\n");
    mgr.tick();
    std::printf("     累计触发 %d 个（期望 3）\n", fired_count);

    std::printf("\n[7] 收尾：手动删掉剩下的 2003(+60s) 和 2004(+50s)\n");
    std::printf("     正确顺序：先从节点取出 user_data，再 del_timer，最后 delete 业务数据。\n");
    std::printf("     反过来的话 del_timer 已经把节点 delete 了，再读 node->user_data 就是 use-after-free。\n");

    client_data *far_data = far->user_data; // 注意：这一行必须写在 del_timer 之前
    mgr.del_timer(far);
    delete far_data;

    client_data *leftover = long_one->user_data;
    mgr.del_timer(long_one);
    delete leftover;
    std::printf("     已清理\n");

    std::printf("\n演示结束：所有节点和业务数据都已释放（可用 -fsanitize=address 复核）。\n");
    return 0;
}
