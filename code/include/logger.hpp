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
#include <cstdarg>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <map>
#include <filesystem>

namespace fs = std::filesystem;

enum class LogLevel {
    DEBUG,
    NEED,
    INFO,
    WARN,
    ERROR,
    FATAL,
    NONE
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
        LogStream& operator<<(const T& val) {
            if(enabled_) oss_ << val;
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
    Logger(const Logger&) = delete; // 禁用拷贝构造
    Logger& operator=(const Logger&) = delete;

    void LoadConfig();

    void ProcessMessage();

    void SetLogMap();

    void SetCurLogPath();

    void CreateLogFile();

    void WriteToFile(const std::string& msg);

    void AddMsgToQue(const std::string& msg);

    void TarLogFile();

    std::string GetTimeFromFileName(const std::string& name);

// 私有成员变量
private:
    std::map<uint64_t, fs::path> log_file_map_;
    std::mutex log_file_map_mutex_;

    std::ofstream log_file_stream_;

    fs::path cur_log_path_;
    size_t cur_file_size_ { 0 };

    LogLevel log_level_ { LogLevel::ERROR };
    size_t file_num_max_ { 10 };
    size_t file_size_max_ { 1024 * 1024 * 10 };
    fs::path log_dir_path_ { "log" };
    bool enable_file_ { false };
    bool enable_console_ { false };

    std::mutex msg_que_mutex_;
    std::condition_variable msg_que_cv_;
    std::queue<std::string> msg_que_;

    std::atomic<bool> is_exit_ { false };

    std::thread process_thread_;
};

#define CDebug   Logger::LogStream(LogLevel::DEBUG,   __FILE__, __func__, __LINE__)
#define CNeed    Logger::LogStream(LogLevel::NEED,    __FILE__, __func__, __LINE__)
#define CInfo    Logger::LogStream(LogLevel::INFO,    __FILE__, __func__, __LINE__)
#define CWarn    Logger::LogStream(LogLevel::WARN,    __FILE__, __func__, __LINE__)
#define CError   Logger::LogStream(LogLevel::ERROR,   __FILE__, __func__, __LINE__)
#define CFatal   Logger::LogStream(LogLevel::FATAL,   __FILE__, __func__, __LINE__)

#define PDebug(fmt, ...)   Logger::LogStream(LogLevel::DEBUG,   __FILE__, __func__, __LINE__).printf(fmt, ##__VA_ARGS__)
#define PNeed(fmt, ...)    Logger::LogStream(LogLevel::NEED,    __FILE__, __func__, __LINE__).printf(fmt, ##__VA_ARGS__)
#define PInfo(fmt, ...)    Logger::LogStream(LogLevel::INFO,    __FILE__, __func__, __LINE__).printf(fmt, ##__VA_ARGS__)
#define PWarn(fmt, ...)    Logger::LogStream(LogLevel::WARN,    __FILE__, __func__, __LINE__).printf(fmt, ##__VA_ARGS__)
#define PError(fmt, ...)   Logger::LogStream(LogLevel::ERROR,   __FILE__, __func__, __LINE__).printf(fmt, ##__VA_ARGS__)
#define PFatal(fmt, ...)   Logger::LogStream(LogLevel::FATAL,   __FILE__, __func__, __LINE__).printf(fmt, ##__VA_ARGS__)

#endif 