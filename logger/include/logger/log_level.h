#pragma once

#include <string>

namespace logger {

/// @brief 日志级别枚举
enum class LogLevel {
    DEBUG = 0,  // 调试信息
    INFO,       // 普通信息
    WARN,       // 警告
    ERROR,      // 错误
    FATAL       // 致命错误（触发 abort）
};

/// @brief 将日志级别转换为可读字符串
/// @param level 日志级别枚举值
/// @return 对应的字符串表示
inline const char* LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "UNKNOWN";
    }
}

}  // namespace logger
