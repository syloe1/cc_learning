/**
 * @file main.cpp
 * @brief SimpleLogger 功能测试 + 多线程压力测试
 *
 * 编译：见 CMakeLists.txt
 * 运行：./build/simple_logger
 */

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "logger/logger.h"

// ============================================================================
// 辅助工具
// ============================================================================

/// @brief 获取当前时间的可读字符串（用于压测报告）
static std::string NowString() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

// ============================================================================
// 基础功能测试
// ============================================================================

void TestBasicFeatures() {
    std::cout << "========== 基础功能测试 ==========\n";

    int uid = 1024;
    const char* username = "Alice";
    double balance = 99.95;

    LOG_DEBUG("调试信息：进入 TestBasicFeatures()");
    LOG_INFO("用户 %d (%s) 登录成功，余额 ¥%.2f", uid, username, balance);
    LOG_WARN("用户 %d 密码即将过期，剩余 3 天", uid);
    LOG_ERROR("用户 %d 操作失败：%s (errno=%d)", uid, "权限不足", 13);
    // 注意：LOG_FATAL 会调用 abort()，此处注释掉以免中断测试
    // LOG_FATAL("系统内存耗尽，即将终止");

    std::cout << "基础功能测试完成，请查看日志文件。\n\n";
}

// ============================================================================
// 多线程压力测试
// ============================================================================

/// @brief 压力测试配置
struct StressConfig {
    int thread_count = 8;          // 并发线程数
    int total_messages = 1000000;  // 总日志条数（100 万）
};

/// @brief 压力测试结果统计
struct StressResult {
    int total_written = 0;                        // 实际写入条数
    double elapsed_sec = 0.0;                     // 总耗时（秒）
    double throughput_msg_per_sec = 0.0;          // 吞吐量（条/秒）
};

StressResult RunStressTest(const StressConfig& cfg) {
    std::cout << "========== 多线程压力测试 ==========\n";
    std::cout << "线程数:     " << cfg.thread_count << "\n";
    std::cout << "目标总量:   " << cfg.total_messages << " 条\n";
    std::cout << "开始时间:   " << NowString() << "\n";

    // 计算每个线程应写入的日志条数
    int per_thread = cfg.total_messages / cfg.thread_count;
    int remainder = cfg.total_messages % cfg.thread_count;

    std::atomic<int> total_written{0};
    std::vector<std::thread> threads;
    threads.reserve(cfg.thread_count);

    auto t_start = std::chrono::steady_clock::now();

    // 启动工作线程
    for (int tid = 0; tid < cfg.thread_count; ++tid) {
        int my_count = per_thread + (tid < remainder ? 1 : 0);

        threads.emplace_back([tid, my_count, &total_written]() {
            for (int i = 0; i < my_count; ++i) {
                // 交替使用不同日志级别，模拟真实场景
                int mod = i % 100;
                if (mod < 70) {
                    // 70% DEBUG
                    LOG_DEBUG("[线程%d] 第 %d 条日志: value=%d, name=%s",
                              tid, i, i * 100, "debug_msg");
                } else if (mod < 90) {
                    // 20% INFO
                    LOG_INFO("[线程%d] 处理记录 %d/%d", tid, i, my_count);
                } else if (mod < 98) {
                    // 8% WARN
                    LOG_WARN("[线程%d] 检测到慢操作，耗时 %dms", tid, i * 7);
                } else {
                    // 2% ERROR
                    LOG_ERROR("[线程%d] 异常 #%d: %s (code=%d)",
                              tid, i, "模拟错误", i % 256);
                }
                ++total_written;
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    auto t_end = std::chrono::steady_clock::now();

    // 统计结果
    StressResult result;
    result.total_written = total_written.load();
    result.elapsed_sec = std::chrono::duration<double>(t_end - t_start).count();
    result.throughput_msg_per_sec =
        result.elapsed_sec > 0.0
            ? static_cast<double>(result.total_written) / result.elapsed_sec
            : 0.0;

    return result;
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║   SimpleLogger — C++20 日志库测试   ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";

    // ---------- 初始化日志系统 ----------
    // 日志目录: ./logs, 基础文件名: app.log, 单文件最大: 50MB
    constexpr size_t kMaxFileSize = 50ULL * 1024 * 1024;  // 50 MB

    try {
        logger::Logger::Instance().Init("./logs", "app.log", kMaxFileSize);
        std::cout << "[OK] 日志系统初始化成功 (目录: ./logs, 单文件上限: 50MB)\n\n";
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] 日志系统初始化失败: " << e.what() << "\n";
        return 1;
    }

    // ---------- 1. 基础功能测试 ----------
    TestBasicFeatures();

    // ---------- 2. 多线程压力测试 ----------
    StressConfig cfg;
    cfg.thread_count = 8;
    cfg.total_messages = 1'000'000;  // 100 万条

    StressResult result = RunStressTest(cfg);

    // ---------- 报告 ----------
    std::cout << "\n========== 压测结果报告 ==========\n";
    std::cout << "结束时间:   " << NowString() << "\n";
    std::cout << "实际写入:   " << result.total_written << " 条\n";
    std::cout << "总耗时:     " << std::fixed << std::setprecision(3)
              << result.elapsed_sec << " 秒\n";
    std::cout << "吞吐量:     " << std::fixed << std::setprecision(0)
              << result.throughput_msg_per_sec << " 条/秒\n";

    if (result.total_written >= cfg.total_messages) {
        std::cout << "状态:       ✓ 全部日志写入完成，无丢失\n";
    } else {
        std::cout << "状态:       ✗ 存在日志丢失 (预期 "
                  << cfg.total_messages << ", 实际 "
                  << result.total_written << ")\n";
    }
    std::cout << "==================================\n";

    // 优雅退出前输出最后一条日志
    LOG_INFO("压测结束，程序正常退出");
    std::cout << "\n日志文件位于 ./logs/ 目录下。\n";

    return 0;
}
