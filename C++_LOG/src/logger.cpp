#include "logger/logger.h"

#include <chrono>
#include <cstring>
#include <ctime>

namespace logger {

// ============================================================================
// Logger::Init —— 初始化日志系统
// ============================================================================

void Logger::Init(const std::string& dir,
                  const std::string& log_name,
                  size_t max_file_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 通过 unique_ptr 接管 LogFile 对象的生命周期（RAII）
    log_file_ = std::make_unique<LogFile>(dir, log_name, max_file_size);
    initialized_ = true;
}

// ============================================================================
// Logger::GetTimestamp —— 获取精确到毫秒的当前时间戳
// ============================================================================

std::string Logger::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::tm tm_now{};
    localtime_r(&time_t_now, &tm_now);  // 线程安全版 localtime

    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             tm_now.tm_year + 1900,
             tm_now.tm_mon + 1,
             tm_now.tm_mday,
             tm_now.tm_hour,
             tm_now.tm_min,
             tm_now.tm_sec,
             static_cast<int>(ms.count()));
    return buf;
}

// ============================================================================
// Logger::MakeLogLine —— 拼接最终日志行
// ============================================================================

std::string Logger::MakeLogLine(LogLevel level,
                                 const char* file,
                                 int line,
                                 const std::string& msg) {
    std::string timestamp = GetTimestamp();
    const char* level_str = LogLevelToString(level);

    // 从完整路径中提取纯文件名
    const char* filename = file;
    const char* last_slash = strrchr(file, '/');
    const char* last_bslash = strrchr(file, '\\');
    if (last_slash != nullptr) filename = last_slash + 1;
    if (last_bslash != nullptr && last_bslash > last_slash)
        filename = last_bslash + 1;

    // 使用栈上合理大小的缓冲区拼接日志行
    // 格式：[时间戳] [级别(左对齐5宽)] [文件名:行号] 日志内容
    char line_buf[4096];
    int written = snprintf(line_buf, sizeof(line_buf),
                           "[%s] [%-5s] [%s:%d] %s\n",
                           timestamp.c_str(),
                           level_str,
                           filename,
                           line,
                           msg.c_str());
    if (written < 0) {
        return "";  // 格式化失败，返回空串
    }
    return line_buf;
}

}  // namespace logger
