#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <string>
#include <iomanip>
#include "ring_buffer.h"

using namespace std::chrono;

// ============================================================================
// 辅助：打印性能统计
// ============================================================================
template <typename Duration>
void print_stats(const std::string& label, size_t total_ops, Duration elapsed) {
    double seconds = duration<double>(elapsed).count();
    double throughput = static_cast<double>(total_ops) / seconds;
    std::cout << std::left << std::setw(36) << label
              << " | ops: " << std::setw(12) << total_ops
              << " | time: " << std::setw(8) << std::fixed << std::setprecision(3) << seconds << " s"
              << " | throughput: " << std::setw(14) << std::fixed << std::setprecision(0) << throughput << " ops/s"
              << std::endl;
}

// ============================================================================
// 演示 SPSCQueue
// ============================================================================
void demo_spsc() {
    std::cout << "============================================\n";
    std::cout << "  SPSCQueue 演示\n";
    std::cout << "============================================\n\n";

    constexpr size_t CAP = 1024;
    SPSCQueue<int, CAP> queue;

    // ---- 单元素操作 ----
    std::cout << "--- 单元素入队/出队 ---\n";
    for (int i = 0; i < 5; ++i) {
        queue.enqueue(i * 10);
        std::cout << "  enqueue(" << i * 10 << ")\n";
    }
    for (int i = 0; i < 5; ++i) {
        int val = 0;
        queue.dequeue(val);
        std::cout << "  dequeue -> " << val << "\n";
    }

    // ---- 批量操作性能测试 ----
    std::cout << "\n--- 批量操作 (1 000 000 元素) ---\n";
    constexpr size_t N = 1'000'000;
    constexpr size_t BULK = 256;

    // 单独线程消费
    std::vector<int> results(N);

    auto t0 = high_resolution_clock::now();

    std::thread consumer([&]() {
        size_t remaining = N;
        size_t offset = 0;
        while (remaining > 0) {
            size_t cnt = (remaining < BULK) ? remaining : BULK;
            size_t got = queue.dequeue_bulk(&results[offset], cnt);
            offset += got;
            remaining -= got;
        }
    });

    // 主线程生产
    std::vector<int> data(BULK);
    size_t remaining = N;
    size_t seq = 0;
    while (remaining > 0) {
        size_t cnt = (remaining < BULK) ? remaining : BULK;
        for (size_t i = 0; i < cnt; ++i) {
            data[i] = static_cast<int>(seq++);
        }
        size_t put = 0;
        while (put < cnt) {
            put += queue.enqueue_bulk(&data[put], cnt - put);
        }
        remaining -= cnt;
    }

    consumer.join();
    auto t1 = high_resolution_clock::now();

    // 验证正确性
    bool ok = true;
    for (size_t i = 0; i < N; ++i) {
        if (results[i] != static_cast<int>(i)) {
            ok = false;
            std::cerr << "  SPSC 数据错误 @" << i << ": 期望 " << i << " 实际 " << results[i] << "\n";
            break;
        }
    }
    std::cout << "  正确性: " << (ok ? "PASS" : "FAIL") << "\n";
    print_stats("  SPSC 批量吞吐", N, t1 - t0);

    std::cout << std::endl;
}

// ============================================================================
// 演示 MPMCLockedQueue
// ============================================================================
void demo_mpmc() {
    std::cout << "============================================\n";
    std::cout << "  MPMCLockedQueue 演示\n";
    std::cout << "============================================\n\n";

    constexpr size_t CAP   = 4096;
    constexpr size_t N     = 1'000'000;  // 总操作数
    constexpr int    PRODS = 4;
    constexpr int    CONS  = 4;

    MPMCLockedQueue<int, CAP> queue;

    // 每个线程操作的数目
    constexpr size_t per_thread = N / PRODS;

    std::vector<int> consumed(N, -1);
    std::atomic<size_t> cons_offset{0};

    auto t0 = high_resolution_clock::now();

    // ---- 启动生产者 ----
    std::vector<std::thread> producers;
    for (int p = 0; p < PRODS; ++p) {
        producers.emplace_back([&, p]() {
            for (size_t i = 0; i < per_thread; ++i) {
                int val = static_cast<int>(p * per_thread + i);
                while (!queue.enqueue(val)) {
                    // 队列满则自旋重试
                }
            }
        });
    }

    // ---- 启动消费者 ----
    std::vector<std::thread> consumers;
    for (int c = 0; c < CONS; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                int val = 0;
                if (queue.dequeue(val)) {
                    size_t off = cons_offset.fetch_add(1, std::memory_order_relaxed);
                    if (off < N) {
                        consumed[off] = val;
                    }
                    if (off + 1 >= N) break;
                } else if (cons_offset.load(std::memory_order_relaxed) >= N) {
                    // 所有元素已消费完毕
                    break;
                }
            }
        });
    }

    // 等待所有线程结束
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto t1 = high_resolution_clock::now();

    // ---- 验证 ----
    // 计算消费总和
    long long sum = 0;
    long long expected_sum = 0;
    for (size_t i = 0; i < N; ++i) {
        sum += consumed[i];
        expected_sum += static_cast<long long>(i);
    }
    std::cout << "  消费总和: " << sum << " (期望 " << expected_sum << ")\n";
    std::cout << "  正确性: " << (sum == expected_sum ? "PASS" : "FAIL") << "\n";
    print_stats("  MPMC 总吞吐", N, t1 - t0);

    std::cout << std::endl;
}

// ============================================================================
int main() {
    std::cout << "Ring Buffer 演示与性能测试\n";
    std::cout << "============================\n\n";

    demo_spsc();
    demo_mpmc();

    return 0;
}
