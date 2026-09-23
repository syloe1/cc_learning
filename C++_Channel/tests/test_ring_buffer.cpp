#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include "ring_buffer.h"

// ============================================================================
// 测试 1: SPSCQueue 单生产者单消费者 — 100 万元素
// ============================================================================
void test_spsc_1m() {
    std::cout << "[TEST] SPSCQueue 单生产者/单消费者 1,000,000 元素 ... ";

    constexpr size_t CAP = 131072;  // 2^17, > 1M 没问题
    constexpr size_t N   = 1'000'000;

    SPSCQueue<int, CAP> queue;
    std::vector<int> results(N, -1);

    // 生产者线程
    std::thread producer([&]() {
        for (size_t i = 0; i < N; ++i) {
            int val = static_cast<int>(i);
            while (!queue.enqueue(val)) {
                // 队列满，自旋等待
            }
        }
    });

    // 消费者线程
    std::thread consumer([&]() {
        for (size_t i = 0; i < N; ++i) {
            int val = -1;
            while (!queue.dequeue(val)) {
                // 队列空，自旋等待
            }
            results[i] = val;
        }
    });

    producer.join();
    consumer.join();

    // 验证
    for (size_t i = 0; i < N; ++i) {
        assert(results[i] == static_cast<int>(i) &&
               "SPSC 1M: 元素不匹配");
    }

    std::cout << "PASS" << std::endl;
}

// ============================================================================
// 测试 2: SPSCQueue 批量操作 — 每次 10 个元素
// ============================================================================
void test_spsc_bulk() {
    std::cout << "[TEST] SPSCQueue 批量入队/出队 (每次 10 个) ... ";

    constexpr size_t CAP  = 256;
    constexpr size_t N    = 1000;       // 100 批次 × 10
    constexpr size_t BULK = 10;

    SPSCQueue<int, CAP> queue;
    std::vector<int> results(N, -1);

    std::thread producer([&]() {
        int data[BULK];
        size_t sent = 0;
        while (sent < N) {
            size_t cnt = (N - sent < BULK) ? (N - sent) : BULK;
            for (size_t i = 0; i < cnt; ++i) {
                data[i] = static_cast<int>(sent + i);
            }
            size_t put = 0;
            while (put < cnt) {
                put += queue.enqueue_bulk(&data[put], cnt - put);
            }
            sent += cnt;
        }
    });

    std::thread consumer([&]() {
        int out[BULK];
        size_t received = 0;
        while (received < N) {
            size_t cnt = (N - received < BULK) ? (N - received) : BULK;
            size_t got = 0;
            while (got < cnt) {
                got += queue.dequeue_bulk(&out[got], cnt - got);
            }
            for (size_t i = 0; i < got; ++i) {
                results[received + i] = out[i];
            }
            received += got;
        }
    });

    producer.join();
    consumer.join();

    // 验证
    for (size_t i = 0; i < N; ++i) {
        assert(results[i] == static_cast<int>(i) &&
               "SPSC bulk: 元素不匹配");
    }

    std::cout << "PASS" << std::endl;
}

// ============================================================================
// 测试 3: MPMCLockedQueue 4 生产者 4 消费者 — 总计 100 万元素
// ============================================================================
void test_mpmc_4p4c() {
    std::cout << "[TEST] MPMCLockedQueue 4 生产者 / 4 消费者 (1,000,000 元素) ... ";

    constexpr size_t CAP   = 65536;  // 2^16
    constexpr size_t N     = 1'000'000;
    constexpr int    PRODS = 4;
    constexpr int    CONS  = 4;

    MPMCLockedQueue<int, CAP> queue;
    std::vector<int> consumed(N, -1);
    std::atomic<size_t> cons_idx{0};

    // 生产者
    std::vector<std::thread> producers;
    const size_t per_prod = N / PRODS;
    for (int p = 0; p < PRODS; ++p) {
        producers.emplace_back([&, p]() {
            for (size_t i = 0; i < per_prod; ++i) {
                int val = static_cast<int>(p * per_prod + i);
                while (!queue.enqueue(val)) { /* spin */ }
            }
        });
    }

    // 消费者
    std::vector<std::thread> consumers;
    for (int c = 0; c < CONS; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                int val = -1;
                if (queue.dequeue(val)) {
                    size_t idx = cons_idx.fetch_add(1, std::memory_order_relaxed);
                    if (idx < N) {
                        consumed[idx] = val;
                    }
                    if (idx + 1 >= N) break;
                } else if (cons_idx.load(std::memory_order_relaxed) >= N) {
                    // 所有元素已消费完毕，退出
                    break;
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    // 验证：所有值总和应为 sum(0..N-1)
    long long actual_sum = 0;
    long long expected_sum = 0;
    for (size_t i = 0; i < N; ++i) {
        assert(consumed[i] >= 0 && "MPMC: 存在未消费的元素");
        actual_sum += static_cast<long long>(consumed[i]);
        expected_sum += static_cast<long long>(i);
    }

    assert(actual_sum == expected_sum && "MPMC: 总和校验失败");
    std::cout << "PASS (sum = " << actual_sum << ")" << std::endl;
}

// ============================================================================
// 测试 4: 边界条件 — 空队列出队 / 满队列入队
// ============================================================================
void test_edge_cases() {
    std::cout << "[TEST] 边界条件 ... ";

    SPSCQueue<int, 4> q;

    // 空队列出队应返回 false
    int val = 0;
    assert(q.dequeue(val) == false && "空队列出队应返回 false");

    // 填满队列
    assert(q.enqueue(1) == true);
    assert(q.enqueue(2) == true);
    assert(q.enqueue(3) == true);
    assert(q.enqueue(4) == true);

    // 满队列再入队应返回 false
    assert(q.enqueue(5) == false && "满队列入队应返回 false");

    // 出队后应可继续入队
    assert(q.dequeue(val) == true && val == 1);
    assert(q.enqueue(5) == true);

    // 清空
    assert(q.dequeue(val) == true && val == 2);
    assert(q.dequeue(val) == true && val == 3);
    assert(q.dequeue(val) == true && val == 4);
    assert(q.dequeue(val) == true && val == 5);
    assert(q.dequeue(val) == false && "再次出队应返回 false");

    std::cout << "PASS" << std::endl;
}

// ============================================================================
// 测试 5: 静态断言 — Capacity 必须是 2 的幂（编译期检查）
// ============================================================================
// 取消下面注释应导致编译错误：
// SPSCQueue<int, 3> bad_queue;  // 3 不是 2 的幂

// ============================================================================
int main() {
    std::cout << "Ring Buffer 单元测试\n";
    std::cout << "====================\n\n";

    test_edge_cases();
    test_spsc_bulk();
    test_spsc_1m();
    test_mpmc_4p4c();

    std::cout << "\n所有测试通过!\n";
    return 0;
}
