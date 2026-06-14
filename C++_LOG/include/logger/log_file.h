#pragma once

#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>

namespace logger {

/// @brief RAII 日志文件管理类
///
/// 封装 FILE* 句柄的生命周期管理，提供按文件大小和按天滚动的能力。
/// - 构造时打开（或创建）日志文件
/// - 析构时自动关闭文件句柄（RAII）
/// - 禁用拷贝，实现移动语义（noexcept）
/// - 内部自带互斥锁，保证文件 I/O 线程安全
class LogFile {
public:
    /// @brief 构造函数：打开指定目录下的日志文件
    /// @param dir        日志存放目录（自动创建）
    /// @param base_name  基础文件名（如 "app.log"）
    /// @param max_size   单文件最大字节数（超出触发大小滚动）
    LogFile(const std::string& dir,
            const std::string& base_name,
            size_t max_size);

    /// @brief 析构函数：RAII 关闭文件句柄
    ~LogFile();

    // ---- 移动语义（noexcept） ----
    //noexcept表示不会抛出异常
    LogFile(LogFile&& other) noexcept;
    LogFile& operator=(LogFile&& other) noexcept;

    // ---- 禁止拷贝 ----
    LogFile(const LogFile&) = delete;
    LogFile& operator=(const LogFile&) = delete;

    /// @brief 线程安全写入日志内容
    /// @param msg 已格式化好的单行日志文本
    void Write(const std::string& msg);

    /// @brief 检查并触发日志滚动（大小 / 日期）
    ///
    /// 满足任一条件即执行滚动：
    /// 1. 当前文件大小 >= max_size_
    /// 2. 当前日期与打开文件的日期不同（跨天）
    void CheckRotate();

private:
    /// @brief 生成带日期后缀的日志文件名（如 base_20260608.log）
    std::string MakeFileName() const;

    /// @brief 执行滚动：关闭旧文件、重命名加时间戳、创建新文件
    /// @note 调用者必须已持有 mutex_
    void RotateFileLocked();

    /// @brief 以追加模式打开日志文件
    /// @note 调用者必须已持有 mutex_
    void OpenFileLocked();

    /// @brief 关闭当前文件句柄（幂等）
    void CloseFileLocked();

    FILE* file_ = nullptr;          ///< 当前日志文件句柄
    std::string dir_;               ///< 日志目录路径
    std::string base_name_;         ///< 基础文件名
    size_t max_size_ = 0;           ///< 单文件最大字节数
    int current_day_ = -1;          ///< 当前文件对应的年中天数（-1 表示未初始化）
    std::mutex mutex_;              ///< 文件操作互斥锁
};

}  // namespace logger
