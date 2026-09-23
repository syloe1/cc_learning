#pragma once

#include "log_file.h"
#include "log_level.h"

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace logger {

/// @brief 全局单例日志主类
///
/// 使用 Meyer's Singleton 模式，线程安全（C++11 起保证静态局部变量初始化安全）。
/// 提供可变参数模板 Log 方法，通过对外宏 LOG_DEBUG / LOG_INFO 等便捷调用。
///
/// 使用示例：
/// @code
///   logger::Logger::Instance().Init("./logs", "app.log", 10 * 1024 * 1024);
///   LOG_INFO("用户 %d 登录成功", uid);
///   LOG_ERROR("文件 %s 打开失败: %s", path, strerror(errno));
/// @endcode
class Logger {
public:
    /// @brief 获取全局单例实例（Meyer's Singleton）
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    /// @brief 初始化日志系统
    /// @param dir           日志文件存放目录
    /// @param log_name      基础日志文件名（如 "app.log"）
    /// @param max_file_size 单文件最大字节数（超出触发大小滚动）
    void Init(const std::string& dir,
              const std::string& log_name,
              size_t max_file_size);

    /// @brief 核心日志输出方法（可变参数模板）
    ///
    /// 利用 C++20 可变参数模板 + 参数包展开，将格式化参数转发给 snprintf
    /// 生成业务日志内容，再拼接时间戳、日志级别、源码位置后写入文件。
    ///
    /// @tparam Args   可变参数类型包
    /// @param  level  日志级别
    /// @param  file   源码文件名（由宏传入 __FILE__）
    /// @param  line   源码行号（由宏传入 __LINE__）
    /// @param  fmt    C 风格格式化字符串
    /// @param  args   可变格式化参数
    template <typename... Args>
    void Log(LogLevel level,
             const char* file,
             int line,
             const char* fmt,
             Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_ || !log_file_) {
            return;
        }

        // 第一步：计算格式化后所需缓冲区大小（snprintf 以 nullptr, 0 探测）
        // fmt 来自调用方宏展开，非编译期字面量，此处抑制 -Wformat-security
        _Pragma("GCC diagnostic push")
        _Pragma("GCC diagnostic ignored \"-Wformat-security\"")
        //snprintf(nullptr, 0, ...)：只计算最终字符串长度，不写入内容；
        int msg_len = snprintf(nullptr, 0, fmt, args...);
        _Pragma("GCC diagnostic pop")
        if (msg_len < 0) {
            return;  // 格式化失败，静默丢弃
        }

        // 第二步：在堆上分配精确大小的缓冲区，避免栈溢出
        _Pragma("GCC diagnostic push")
        _Pragma("GCC diagnostic ignored \"-Wformat-security\"")
        std::vector<char> msg_buf(static_cast<size_t>(msg_len) + 1);
        snprintf(msg_buf.data(), msg_buf.size(), fmt, args...);
        _Pragma("GCC diagnostic pop")

        // 第三步：拼接完整日志行 [时间戳] [级别] [文件:行号] 内容
        std::string log_line = MakeLogLine(level, file, line, msg_buf.data());

        // 第四步：检查滚动条件，然后写入文件
        log_file_->CheckRotate();
        log_file_->Write(log_line);

        // 第五步：FATAL 级别强制刷新所有流并终止进程
        if (level == LogLevel::FATAL) {
            fflush(nullptr);
            std::abort();
        }
    }

private:
    Logger() = default;
    ~Logger() = default;

    // 禁止拷贝和移动（单例）
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /// @brief 获取当前时间戳字符串（精确到毫秒）
    std::string GetTimestamp() const;

    /// @brief 拼接完整日志行
    /// @return "[2026-06-08 14:30:25.123] [INFO ] [main.cpp:42] 用户 123 登录"
    std::string MakeLogLine(LogLevel level,
                            const char* file,
                            int line,
                            const std::string& msg);

    std::unique_ptr<LogFile> log_file_;  ///< 日志文件对象（RAII 管理）
    std::mutex mutex_;                   ///< 日志主类互斥锁（保护整体写流程）
    bool initialized_ = false;           ///< 是否已初始化
};

}  // namespace logger

// ============================================================================
// 对外日志宏 —— 模仿经典日志库调用风格
// ============================================================================
// __VA_ARGS__ 展开后直接传递给 Logger::Log 的可变参数模板
// fmt 作为第一个固定参数，剩余参数填充参数包
// ##__VA_ARGS__ 为 GNU 扩展：当可变参数为空时吞噬前导逗号

#define LOG_DEBUG(fmt, ...)                                       \
    logger::Logger::Instance().Log(logger::LogLevel::DEBUG,       \
                                    __FILE__, __LINE__,           \
                                    fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...)                                        \
    logger::Logger::Instance().Log(logger::LogLevel::INFO,        \
                                    __FILE__, __LINE__,           \
                                    fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...)                                        \
    logger::Logger::Instance().Log(logger::LogLevel::WARN,        \
                                    __FILE__, __LINE__,           \
                                    fmt, ##__VA_ARGS__)
//__VA_ARGS__ 可变参数
//##__VA_ARGS__ GNU 扩展语法解决无可变参数场景：
#define LOG_ERROR(fmt, ...)                                       \
    logger::Logger::Instance().Log(logger::LogLevel::ERROR,       \
                                    __FILE__, __LINE__,           \
                                    fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...)                                       \
    logger::Logger::Instance().Log(logger::LogLevel::FATAL,       \
                                    __FILE__, __LINE__,           \
                                    fmt, ##__VA_ARGS__)
