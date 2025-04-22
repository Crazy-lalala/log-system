#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <mutex>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <cstdarg>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <map>

#if defined(__i386__) || defined(__x86_64__)
    #include <filesystem>
    using fs = std::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

// Logger 类，用于 输出日志信息
class Logger {

// 公有函数
public:
    static Logger& GetInstance();

// 公有类
public:
    // LogStream 类，用于 打印信息的保存和获取
    class LogStream {
    // 公有函数
    public:
        LogStream(LogLevel level, const char* file, const char* func, const int line);
        ~LogStream();

        // 重载 << 操作符 以支持 基本数据元素 输入
        template <typename T>
        LogStream& operator<<(const T& value) {
            if(enabled_) oss_ << value;
            return *this;
        }

        // 重载 << 操作符 以支持 std::endl等 ostream&操作符 输入
        LogStream& operator << (std::ostream& (*func)(std::ostream&));

        // 提供 printf 风格的输出
        void Printf(const char* format, ...);
    
    //私有成员变量
    private:
        std::ostringstream oss_;
        bool enabled_;
    };

// 私有函数
private:
    Logger();
    ~Logger();

    void LoadConfig();

    void ProcessMessage();

    void SetLogMap();

    void SetCurLogPath();

    void CreateLogFile();

    void WriteToFile(const std::string& message);

    void TarLogFile();

// 私有成员变量
private:
    std::map<uint64_t, fs::path> log_map_;
    std::mutex log_map_mutex_;

    std::ostream log_file_stream_;

    fs::path cur_log_path_;
    size_t cur_file_size_ { 0 };

    LogLevel log_level_ { LogLevel::ERROR };
};

#endif 