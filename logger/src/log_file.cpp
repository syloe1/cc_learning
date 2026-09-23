#include "logger/log_file.h"

#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace logger {

// ============================================================================
// 构造 / 析构
// ============================================================================

LogFile::LogFile(const std::string& dir,
                 const std::string& base_name,
                 size_t max_size)
    : dir_(dir)
    , base_name_(base_name)
    , max_size_(max_size) {
    // 自动创建日志目录（使用 std::filesystem）
    std::error_code ec;
    if (!std::filesystem::exists(dir_)) {
        if (!std::filesystem::create_directories(dir_, ec)) {
            throw std::runtime_error("无法创建日志目录: " + dir_ +
                                     " (" + ec.message() + ")");
        }
    }

    // 初始化当前日期
    //std::time(nullptr)
    std::time_t now = std::time(nullptr);
    std::tm tm_now{}; //创建时间结构体
    localtime_r(&now, &tm_now); //秒数->本地时区的时间结构体
    current_day_ = tm_now.tm_yday;

    // 打开日志文件
    OpenFileLocked();
}

LogFile::~LogFile() {
    CloseFileLocked();
}

// ============================================================================
// 移动语义
// ============================================================================

LogFile::LogFile(LogFile&& other) noexcept
    : file_(other.file_)
    , dir_(std::move(other.dir_))
    , base_name_(std::move(other.base_name_))
    , max_size_(other.max_size_)
    , current_day_(other.current_day_) {
    // 将源对象置于可安全析构状态
    other.file_ = nullptr;
    other.current_day_ = -1;
}

LogFile& LogFile::operator=(LogFile&& other) noexcept {
    //判断当前对象和源对象是不是同一个对象。
    if (this != &other) {
        // 先关闭自身持有的文件
        CloseFileLocked();

        // 转移资源
        file_ = other.file_;
        dir_ = std::move(other.dir_);
        base_name_ = std::move(other.base_name_);
        max_size_ = other.max_size_;
        current_day_ = other.current_day_;

        // 源对象安全置空
        other.file_ = nullptr;
        other.current_day_ = -1;
    }
    return *this;
}

// ============================================================================
// 公开接口
// ============================================================================

void LogFile::Write(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_ == nullptr) {
        return;  // 文件未打开，静默丢弃（不应发生）
    }
    /*
    size_t fwrite(
        const void *ptr,   // 数据源起始地址
        size_t size,       // 单个数据块字节数
        size_t count,      // 数据块个数
        FILE *stream       // 目标C文件流
    );
     */
    fwrite(msg.data(), 1, msg.size(), file_);
    fflush(file_);  // 立即落盘，保证崩溃时日志不丢失
}

void LogFile::CheckRotate() {
    std::lock_guard<std::mutex> lock(mutex_);

    bool need_rotate = false;

    // 条件 1：按天滚动 —— 检测当前日期是否与打开文件时不同
    std::time_t now = std::time(nullptr);
    std::tm tm_now{};
    localtime_r(&now, &tm_now);
    int today = tm_now.tm_yday;
    // ！= -1， 文件已日常打开
    if (current_day_ != -1 && current_day_ != today) {
        need_rotate = true;
    }

    // 条件 2：按大小滚动 —— 检测当前文件是否超出阈值
    if (file_ != nullptr && ftell(file_) >= static_cast<long>(max_size_)) {
        need_rotate = true;
    }

    if (need_rotate) {
        RotateFileLocked();
        current_day_ = today;
        OpenFileLocked();
    }
}

// ============================================================================
// 内部实现
// ============================================================================

std::string LogFile::MakeFileName() const {
    // 文件名格式：base_YYYYMMDD.log
    std::time_t now = std::time(nullptr);
    std::tm tm_now{};
    localtime_r(&now, &tm_now);

    char date_suffix[32];
    snprintf(date_suffix, sizeof(date_suffix),
             "%04d%02d%02d",
             tm_now.tm_year + 1900,
             tm_now.tm_mon + 1,
             tm_now.tm_mday);

    // 在基础名中插入日期后缀
    // 如 "app.log" → "app_20260608.log"
    size_t dot_pos = base_name_.rfind('.');
    if (dot_pos != std::string::npos) {
        return base_name_.substr(0, dot_pos) + "_" + date_suffix +
               base_name_.substr(dot_pos);
    }
    return base_name_ + "_" + date_suffix;
}

void LogFile::RotateFileLocked() {
    // 关闭当前文件
    if (file_ != nullptr) {
        std::string old_path = dir_ + "/" + MakeFileName();

        fclose(file_);
        file_ = nullptr;

        // 如果旧文件存在，重命名为带时间戳的归档文件名
        if (std::filesystem::exists(old_path)) {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::tm tm_now{};
            localtime_r(&time_t_now, &tm_now);

            char ts[32];
            snprintf(ts, sizeof(ts), "%02d%02d%02d",
                     tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);

            // 归档名：app_20260608.log → app_20260608_202959.log
            size_t dot_pos = old_path.rfind('.');
            std::string archive_path;
            if (dot_pos != std::string::npos) {
                archive_path = old_path.substr(0, dot_pos) + "_" + ts +
                               old_path.substr(dot_pos);
            } else {
                archive_path = old_path + "_" + ts;
            }

            std::rename(old_path.c_str(), archive_path.c_str());
        }
    }
}

void LogFile::OpenFileLocked() {
    // 确保目录存在
    std::error_code ec;
    if (!std::filesystem::exists(dir_)) {
        std::filesystem::create_directories(dir_, ec);
    }

    std::string file_path = dir_ + "/" + MakeFileName();

    // 以追加模式打开（不存在则创建）
    file_ = fopen(file_path.c_str(), "a");
    if (file_ == nullptr) {
        throw std::runtime_error("无法打开日志文件: " + file_path);
    }
}

void LogFile::CloseFileLocked() {
    if (file_ != nullptr) {
        fclose(file_);
        file_ = nullptr;
    }
}

}  // namespace logger
