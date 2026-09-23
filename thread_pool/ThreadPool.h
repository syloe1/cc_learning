/**
 * ThreadPool.h — 简易 C++ 通用线程池（Header-only 纯头文件实现，基于 C++11）
 *
 * 核心特性：
 *  1. 固定核心线程 + 动态伸缩线程：队列堆积自动扩容，空闲超时自动缩容
 *  2. 优先级任务队列：优先级数值越小，任务执行优先级越高
 *  3. 异步任务提交：submit 返回 std::future，支持获取返回值 & 异常透传
 *  4. 两种关闭策略：优雅关闭(执行完队列任务) / 强制关闭(丢弃未执行任务)
 *  5. 全线程安全，任务在锁外运行，减少锁竞争
 *  6. 禁止拷贝/移动，生命周期由使用者管理
 *
 * 编译依赖：C++11 及以上标准库
 *  头文件依赖：<thread> <mutex> <condition_variable> <functional> <future>
 *             <queue> <vector> <stdexcept> <atomic> <chrono> <memory> 等
 *
 * 使用约束：
 *  - 核心线程数必须大于 0
 *  - 关闭后不再接受新任务，重复关闭安全无副作用
 *  - 动态缩容线程采用 detach 方式，不影响整体 join 逻辑
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

/**
 * @brief 动态伸缩优先级线程池
 */
class ThreadPool {
public:
  // ==============================================
  // 构造函数 & 析构函数
  // ==============================================

  /**
   * @brief 构造线程池，初始化核心工作线程
   * @param coreThreads 常驻核心线程数，不可缩容，必须 > 0
   * @param maxThreads 最大线程上限；传 0 则等价于 coreThreads，关闭动态伸缩能力
   * @throw std::invalid_argument 当 coreThreads 为 0 时抛出异常
   */
  /*
      ==============================================
      成员变量
      ==============================================
     size_t                           core_threads_;  ///<
     常驻核心线程数（最小线程数） size_t                           max_threads_;
     ///< 线程池最大线程上限 std::vector<std::thread>         workers_; ///<
     所有工作线程容器 std::priority_queue<PrioTask>    tasks_;         ///<
     优先级任务队列（大顶堆实现优先级） mutable std::mutex               mtx_;
     ///< 全局互斥锁（状态查询为 const，故 mutable） std::condition_variable
     cv_;           ///< 条件变量：唤醒等待任务的线程 bool stop_;          ///<
     全局停止标记：true 表示线程池开始关闭流程 bool
 */
  // explicit禁止隐式转换
  explicit ThreadPool(size_t coreThreads, size_t maxThreads = 0)
      : core_threads_(coreThreads),
        max_threads_(maxThreads > 0 ? maxThreads : coreThreads), stop_(false),
        accepting_(true) {
    // 核心线程数不允许为 0
    if (coreThreads == 0) {
      throw std::invalid_argument("ThreadPool: coreThreads must be > 0");
    }

    // 预先创建所有核心工作线程
    for (size_t i = 0; i < core_threads_; ++i) {
      add_thread();
    }
  }

  /**
   * @brief 析构函数
   *  若线程池未正常关闭，则执行【立即关闭】：丢弃队列剩余任务、终止所有线程
   */
  ~ThreadPool() {
    // 仍在运行/还有活跃线程，强制关闭
    if (!stop_ || !workers_.empty()) {
      stop_immediately();
    }
  }

  // 禁止拷贝构造 & 赋值运算符，线程池不可拷贝
  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  // ==============================================
  // 任务提交接口
  // ==============================================

  /**
   * @brief 提交普通任务（默认最低优先级）
   * @tparam F 可调用对象类型（函数、lambda、仿函数等）
   * @tparam Args 任务参数类型列表
   * @param f 待执行任务
   * @param args 任务入参
   * @return std::future 异步结果句柄，可 get() 阻塞取值/捕获异常
   * @throw std::runtime_error 线程池已关闭，拒绝提交新任务
   */
  template <class F, class... Args>
  auto submit(F &&f, Args &&...args)
      -> std::future<typename std::result_of<F(Args...)>::type> {
    // 数值最大代表最低优先级
    return submit_impl(std::numeric_limits<int>::max(), std::forward<F>(f),
                       std::forward<Args>(args)...);
  }

  /**
   * @brief 提交带自定义优先级的任务
   * @param priority 优先级：数值越小优先级越高，0 为最高优先级
   * @tparam F 可调用对象类型
   * @tparam Args 任务参数类型列表
   * @param f 待执行任务
   * @param args 任务入参
   * @return std::future 异步结果句柄
   * @throw std::runtime_error 线程池已关闭，拒绝提交新任务
   */
  template <class F, class... Args>
  auto submit(int priority, F &&f, Args &&...args)
      -> std::future<typename std::result_of<F(Args...)>::type> {
    return submit_impl(priority, std::forward<F>(f),
                       std::forward<Args>(args)...);
  }

  // ==============================================
  // 线程池关闭接口
  // ==============================================

  /**
   * @brief 优雅关闭线程池
   *  1. 停止接受新任务
   *  2. 保证队列中**所有已提交任务全部执行完毕**
   *  3. 等待所有工作线程退出并 join
   *  4. 重复调用安全，不会重复执行关闭逻辑
   */
  void wait_and_stop() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      // 已处于关闭状态，直接返回
      if (!accepting_)
        return;
      accepting_ = false;
      stop_ = true;
    }
    // 唤醒所有阻塞等待的工作线程，开始收尾
    cv_.notify_all();
    // 等待所有线程执行完毕并回收
    join_all_workers();
  }

  /**
   * @brief 立即强制关闭线程池
   *  1. 停止接受新任务
   *  2. **直接清空任务队列**，未执行任务全部丢弃
   *  3. 唤醒并终止所有工作线程
   *  4. 重复调用安全
   */
  void stop_immediately() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (!accepting_ && stop_)
        return;

      accepting_ = false;
      // 交换空队列，快速清空待执行任务
      std::priority_queue<PrioTask> empty_queue;
      std::swap(tasks_, empty_queue);
      stop_ = true;
    }
    cv_.notify_all();
    join_all_workers();
  }

  // ==============================================
  // 状态查询接口（线程安全）
  // ==============================================

  /**
   * @brief 获取当前活跃工作线程总数（核心+扩容线程）
   * @return 线程数量
   */
  size_t active_threads() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return workers_.size();
  }

  /**
   * @brief 获取当前队列中等待执行的任务数量
   * @return 待执行任务数
   */
  size_t pending_tasks() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return tasks_.size();
  }

private:
  // ==============================================
  // 内部：优先级任务结构体
  // ==============================================

  /**
   * @brief 封装单个优先级任务
   */
  struct PrioTask {
    int priority;               ///< 任务优先级，越小越优先
    std::function<void()> func; ///< 包装后的无参任务函数

    /**
     * @brief 重载比较运算符，适配 std::priority_queue（默认大顶堆）
     *  规则：this->priority > other.priority 时，this 排在后面
     *  最终效果：**优先级数值越小，越优先被取出执行**
     */
    bool operator<(const PrioTask &other) const {
      return this->priority > other.priority;
    }
  };

  // ==============================================
  // 内部：任务提交统一实现
  // ==============================================

  /**
   * @brief 任务提交底层实现
   * @param priority 任务优先级
   * @param f 原始可调用对象
   * @param args 任务参数
   * @return 任务异步 future
   */
  template <class F, class... Args>
  auto submit_impl(int priority, F &&f, Args &&...args)
      -> std::future<typename std::result_of<F(Args...)>::type> {
    using ReturnType = typename std::result_of<F(Args...)>::type;

    // packaged_task 封装任务，支持异步取值 & 异常传递
    // 使用 shared_ptr 管理：packaged_task 不可拷贝，多场景安全传递
    auto task_ptr = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    // 提前获取 future，返回给调用方
    std::future<ReturnType> result = task_ptr->get_future();

    {
      std::lock_guard<std::mutex> lock(mtx_);

      // 线程池已关闭，禁止提交新任务
      if (!accepting_) {
        throw std::runtime_error(
            "ThreadPool: cannot submit — pool is shutting down");
      }

      // 构造优先级任务并入队
      PrioTask pt;
      pt.priority = priority;
      // 捕获 shared_ptr，任务执行时调用 packaged_task
      pt.func = [task_ptr]() { (*task_ptr)(); };
      tasks_.push(std::move(pt));

      // 动态扩容逻辑：任务数 > 当前线程数 且 未达到最大线程上限 → 新增线程
      if (tasks_.size() > workers_.size() && workers_.size() < max_threads_) {
        add_thread();
      }
    }

    // 唤醒一个阻塞的工作线程去执行任务
    cv_.notify_one();
    return result;
  }

  // ==============================================
  // 内部：工作线程主循环（核心执行逻辑）
  // ==============================================

  /**
   * @brief 每个工作线程的执行入口函数，循环取任务、执行任务
   *  逻辑：取任务 → 释放锁 → 执行任务 → 循环
   *  动态伸缩：空闲超时 1s 且线程数大于核心数，则线程自动退出(缩容)
   */
  void worker_loop() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mtx_);

        // 队列为空 + 未停止 → 进入等待状态
        if (tasks_.empty() && !stop_) {
          // 开启了动态伸缩（最大线程 > 核心线程）：超时等待，用于缩容
          if (max_threads_ > core_threads_) {
            // 空闲等待 1 秒
            auto timeout = std::chrono::milliseconds(1000);
            bool has_task = cv_.wait_for(
                lock, timeout, [this] { return stop_ || !tasks_.empty(); });

            // 等待超时、无任务、且当前线程数超过核心数 → 当前线程退出（缩容）
            if (!has_task && workers_.size() > core_threads_) {
              remove_this_thread(lock);
              return;
            }
            // 超时但已是最小核心线程数，继续循环等待任务
            if (!has_task) {
              continue;
            }
          } else {
            // 固定线程池：无限等待任务/停止信号
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
          }
        }

        // 收到停止信号 + 队列已空 → 线程正常退出
        if (stop_ && tasks_.empty()) {
          return;
        }

        // 取出优先级最高的任务（堆顶）
        task = std::move(tasks_.top().func);
        tasks_.pop();
      } // 离开作用域，自动释放互斥锁

      // ========== 锁外执行任务，最大化并发性能 ==========
      try {
        task();
      } catch (...) {
        // 任务内部异常会被 packaged_task 捕获，通过 future::get() 抛给调用者
        // 此处仅兜底，防止单个任务异常导致整个工作线程崩溃
      }
    }
  }

  // ==============================================
  // 内部：线程管理工具函数
  // ==============================================

  /**
   * @brief 创建并添加一个新工作线程
   * @note 调用方**必须已持有 mtx_ 锁**
   */
  void add_thread() { workers_.emplace_back(&ThreadPool::worker_loop, this); }

  /**
   * @brief 缩容：将当前执行线程从线程列表中移除并 detach
   * @param lock 当前持有的互斥锁（仅作所有权标记）
   * @note 调用方**必须已持有 mtx_ 锁**
   */
  void remove_this_thread(std::unique_lock<std::mutex> & /*lock*/) {
    std::thread::id my_id = std::this_thread::get_id();
    // 遍历线程容器，找到当前线程并移除
    for (auto it = workers_.begin(); it != workers_.end(); ++it) {
      if (it->get_id() == my_id) {
        it->detach();       // 分离线程，不再需要外部 join
        workers_.erase(it); // 从容器中删除线程对象
        return;
      }
    }
  }

  /**
   * @brief 等待并回收所有工作线程
   *  先拷贝线程列表再 join，避免持锁 join 造成死锁
   */
  void join_all_workers() {
    std::vector<std::thread> to_join;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      // 交换出所有线程，解除容器与线程的关联
      to_join.swap(workers_);
    }
    // 逐个等待线程结束
    for (auto &t : to_join) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  // ==============================================
  // 成员变量
  // ==============================================
  size_t core_threads_;                 ///< 常驻核心线程数（最小线程数）
  size_t max_threads_;                  ///< 线程池最大线程上限
  std::vector<std::thread> workers_;    ///< 所有工作线程容器
  std::priority_queue<PrioTask> tasks_; ///< 优先级任务队列（大顶堆实现优先级）
  mutable std::mutex mtx_;     ///< 全局互斥锁（状态查询为 const，故 mutable）
  std::condition_variable cv_; ///< 条件变量：唤醒等待任务的线程
  bool stop_;                  ///< 全局停止标记：true 表示线程池开始关闭流程
  bool accepting_;             ///< 任务接收标记：true 允许提交新任务
};

#endif // THREADPOOL_H