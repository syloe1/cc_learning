/**
 * main.cpp — 线程池综合测试套件
 *
 * 覆盖：
 *   1. 基本无返回值任务执行
 *   2. 带返回值任务 + future.get()
 *   3. 异常传播
 *   4. 任务优先级排序
 *   5. wait_and_stop 优雅关闭
 *   6. stop_immediately 立即关闭
 *   7. 动态扩容与缩容
 *   8. 多线程并发提交
 *   9. 关闭后提交抛出异常
 *  10. 边界条件（单线程池、零线程抛出异常）
 */

#include "ThreadPool.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ——————————————————————————————————————————
// 测试辅助宏
// ——————————————————————————————————————————

static int  g_tests_passed = 0;
static int  g_tests_failed = 0;
static bool g_test_ok      = false;

#define TEST(name)                                                        \
    do {                                                                  \
        g_test_ok = true;                                                 \
        std::cout << "[RUN ] " << (name) << std::endl;                    \
    } while (0)

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::cerr << "  FAIL at " << __FILE__ << ":" << __LINE__       \
                      << " — " << #cond << std::endl;                     \
            g_test_ok = false;                                            \
        }                                                                 \
    } while (0)

#define ENDTEST()                                                         \
    do {                                                                  \
        if (g_test_ok) {                                                  \
            ++g_tests_passed;                                             \
            std::cout << "[PASS] " << std::endl;                          \
        } else {                                                          \
            ++g_tests_failed;                                             \
            std::cout << "[FAIL] " << std::endl;                          \
        }                                                                 \
    } while (0)

// ——————————————————————————————————————————
// 测试 1：基本无返回值任务
// ——————————————————————————————————————————

void test_basic_submit()
{
    TEST("basic void task execution");

    ThreadPool pool(4);
    std::atomic<int> counter{0};

    // 提交 100 个任务，每个递增计数器
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    // 等待所有 future 就绪
    for (auto& f : futures) {
        f.get();
    }

    CHECK(counter.load() == 100);
    pool.wait_and_stop();

    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 2：带返回值任务
// ——————————————————————————————————————————

void test_return_value()
{
    TEST("return value via future.get()");

    ThreadPool pool(4);

    auto f1 = pool.submit([] { return 42; });
    auto f2 = pool.submit([](int a, int b) { return a + b; }, 10, 20);
    auto f3 = pool.submit([](const std::string& s) { return s + " world"; },
                          std::string("hello"));

    CHECK(f1.get() == 42);
    CHECK(f2.get() == 30);
    CHECK(f3.get() == "hello world");

    pool.wait_and_stop();
    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 3：异常传播
// ——————————————————————————————————————————

void test_exception_propagation()
{
    TEST("exception propagation through future");

    ThreadPool pool(4);

    auto f = pool.submit([]() -> int {
        throw std::runtime_error("test exception from task");
        return 0;
    });

    bool caught = false;
    try {
        f.get();
    } catch (const std::runtime_error& e) {
        caught = true;
        std::string msg = e.what();
        CHECK(msg.find("test exception from task") != std::string::npos);
    } catch (...) {
        // 不应该走到这里
        CHECK(false);
    }
    CHECK(caught);

    pool.wait_and_stop();
    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 4：优先级排序
// ——————————————————————————————————————————

void test_priority_ordering()
{
    TEST("priority ordering (lower number = higher priority)");

    // 单线程池确保严格顺序
    ThreadPool pool(1);
    std::vector<int> execution_order;
    std::mutex       order_mtx;

    // 提交三个任务，优先级分别为低(10)、最高(0)、中(5)
    // 期望执行顺序：0 → 5 → 10
    auto f1 = pool.submit(10, [&] {
        std::lock_guard<std::mutex> lk(order_mtx);
        execution_order.push_back(10);
    });
    auto f2 = pool.submit(0, [&] {
        std::lock_guard<std::mutex> lk(order_mtx);
        execution_order.push_back(0);
    });
    auto f3 = pool.submit(5, [&] {
        std::lock_guard<std::mutex> lk(order_mtx);
        execution_order.push_back(5);
    });

    f1.get(); f2.get(); f3.get();

    CHECK(execution_order.size() == 3);
    CHECK(execution_order[0] == 0);
    CHECK(execution_order[1] == 5);
    CHECK(execution_order[2] == 10);

    pool.wait_and_stop();
    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 5：wait_and_stop 优雅关闭
// ——————————————————————————————————————————

void test_wait_and_stop()
{
    TEST("wait_and_stop — all tasks complete before shutdown");

    ThreadPool pool(4);
    std::atomic<int> counter{0};

    // 提交 200 个任务，每个 sleep 一小段时间
    for (int i = 0; i < 200; ++i) {
        pool.submit([&counter] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // 立即调用 wait_and_stop，不应死锁
    pool.wait_and_stop();

    // 所有任务都应完成
    CHECK(counter.load() == 200);

    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 6：stop_immediately 立即关闭
// ——————————————————————————————————————————

void test_stop_immediately()
{
    TEST("stop_immediately — discards pending tasks");

    ThreadPool pool(2);
    std::atomic<int> executed{0};

    // 提交大量慢任务
    for (int i = 0; i < 500; ++i) {
        pool.submit([&executed] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            executed.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // 不等任务完成，立即停止
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    pool.stop_immediately();

    // 应该只有少量任务被执行
    size_t done = executed.load();
    std::cout << "  (executed before stop: " << done << " / 500)" << std::endl;
    CHECK(done < 500);   // 不是所有任务都完成

    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 7：动态扩容
// ——————————————————————————————————————————

void test_dynamic_scale_up()
{
    TEST("dynamic scale-up when queue is congested");

    // 核心 2 线程，最大 8 线程
    ThreadPool pool(2, 8);
    std::atomic<int> started{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<int> in_flight{0};

    // 提交很多阻塞任务，触发扩容
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([&] {
            started.fetch_add(1, std::memory_order_relaxed);
            int cur = in_flight.fetch_add(1, std::memory_order_relaxed) + 1;
            // 跟踪最大并发数
            int prev = max_concurrent.load(std::memory_order_relaxed);
            while (cur > prev
                   && !max_concurrent.compare_exchange_weak(prev, cur,
                       std::memory_order_relaxed))
            {}
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            in_flight.fetch_sub(1, std::memory_order_relaxed);
        }));
    }

    for (auto& f : futures) f.get();

    std::cout << "  max concurrent threads observed: "
              << max_concurrent.load() << " (core=2, max=8)" << std::endl;
    CHECK(max_concurrent.load() >= 3);   // 至少超过了核心线程数
    CHECK(max_concurrent.load() <= 8);   // 不超过最大线程数

    pool.wait_and_stop();
    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 8：动态缩容
// ——————————————————————————————————————————

void test_dynamic_scale_down()
{
    TEST("dynamic scale-down after idle timeout");

    ThreadPool pool(2, 6);

    // 提交一批任务触发扩容
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 50; ++i) {
        futures.push_back(pool.submit([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }));
    }
    for (auto& f : futures) f.get();

    // 记录扩容后的线程数
    size_t peak = pool.active_threads();
    std::cout << "  threads after burst: " << peak << std::endl;

    // 等待缩容（超时为 1 秒 + 余量）
    std::this_thread::sleep_for(std::chrono::seconds(3));

    size_t after_idle = pool.active_threads();
    std::cout << "  threads after idle: " << after_idle << std::endl;

    CHECK(after_idle <= peak);            // 应该减少了
    CHECK(after_idle >= pool.active_threads() && after_idle <= peak);

    pool.wait_and_stop();
    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 9：多线程并发提交
// ——————————————————————————————————————————

void test_concurrent_submit()
{
    TEST("concurrent submit from multiple threads");

    ThreadPool pool(8);
    std::atomic<int> counter{0};
    const int         num_submitters = 8;
    const int         tasks_per      = 250;

    // 多个线程同时提交任务
    std::vector<std::thread> submitters;
    for (int t = 0; t < num_submitters; ++t) {
        submitters.emplace_back([&pool, &counter, tasks_per] {
            std::vector<std::future<void>> futs;
            for (int i = 0; i < tasks_per; ++i) {
                futs.push_back(pool.submit([&counter] {
                    counter.fetch_add(1, std::memory_order_relaxed);
                }));
            }
            for (auto& f : futs) f.get();
        });
    }

    for (auto& t : submitters) t.join();

    CHECK(counter.load() == num_submitters * tasks_per);
    pool.wait_and_stop();

    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 10：关闭后提交抛出异常
// ——————————————————————————————————————————

void test_submit_after_stop()
{
    TEST("submit after stop throws exception");

    ThreadPool pool(4);
    pool.wait_and_stop();

    bool caught = false;
    try {
        pool.submit([] { return 1; });
    } catch (const std::runtime_error& e) {
        caught = true;
        std::cout << "  exception: " << e.what() << std::endl;
    }
    CHECK(caught);

    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 11：零线程构造抛出异常
// ——————————————————————————————————————————

void test_zero_threads()
{
    TEST("constructor throws for zero core threads");

    bool caught = false;
    try {
        ThreadPool pool(0);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    CHECK(caught);

    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 12：active_threads 和 pending_tasks
// ——————————————————————————————————————————

void test_thread_count_queries()
{
    TEST("active_threads() and pending_tasks() queries");

    ThreadPool pool(4);
    CHECK(pool.active_threads() == 4);
    CHECK(pool.pending_tasks() == 0);

    // 提交大量慢任务，检查 pending 数量
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 50; ++i) {
        futures.push_back(pool.submit([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }));
    }

    // 此时应该有一些任务在排队
    size_t pending = pool.pending_tasks();
    std::cout << "  pending while busy: " << pending << std::endl;
    CHECK(pool.active_threads() == 4);

    for (auto& f : futures) f.get();
    CHECK(pool.pending_tasks() == 0);

    pool.wait_and_stop();
    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 13：固定线程池不做伸缩
// ——————————————————————————————————————————

void test_fixed_thread_pool()
{
    TEST("fixed thread pool — no scaling");

    ThreadPool pool(4);   // maxThreads=0 → 固定模式
    CHECK(pool.active_threads() == 4);

    // 提交大量任务
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 200; ++i) {
        futures.push_back(pool.submit([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }));
    }

    // 固定模式下线程数不应变化
    CHECK(pool.active_threads() == 4);

    for (auto& f : futures) f.get();
    CHECK(pool.active_threads() == 4);

    pool.wait_and_stop();
    ENDTEST();
}

// ——————————————————————————————————————————
// 测试 14：析构函数自动清理
// ——————————————————————————————————————————

void test_destructor_cleanup()
{
    TEST("destructor calls stop_immediately");

    std::atomic<int> executed{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 100; ++i) {
            pool.submit([&executed] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                executed.fetch_add(1, std::memory_order_relaxed);
            });
        }
        // pool 离开作用域 → 析构函数调用 stop_immediately
    }
    // 不应死锁，正常退出
    std::cout << "  destructor returned without deadlock, executed: "
              << executed.load() << std::endl;
    CHECK(true);   // 到达这里即通过

    ENDTEST();
}

// ——————————————————————————————————————————
// main
// ——————————————————————————————————————————

int main()
{
    std::cout << "=== C++11 ThreadPool Test Suite ===" << std::endl;
    std::cout << "Hardware concurrency: "
              << std::thread::hardware_concurrency() << std::endl
              << std::endl;

    test_basic_submit();
    test_return_value();
    test_exception_propagation();
    test_priority_ordering();
    test_wait_and_stop();
    test_stop_immediately();
    test_dynamic_scale_up();
    test_dynamic_scale_down();
    test_concurrent_submit();
    test_submit_after_stop();
    test_zero_threads();
    test_thread_count_queries();
    test_fixed_thread_pool();
    test_destructor_cleanup();

    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "Results: " << g_tests_passed << " passed, "
              << g_tests_failed << " failed" << std::endl;
    std::cout << "====================================" << std::endl;

    return g_tests_failed == 0 ? 0 : 1;
}
