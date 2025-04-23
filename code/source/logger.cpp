#include <regex>
#include <cstdlib>
#include <unordered_map>
#include "logger.hpp"

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    LoadConfig();
    fs::create_directories(log_dir_path_);

    {
        std::lock_guard<std::mutex> lock(log_file_map_mutex_);
        SetLogMap();
        SetCurLogPath();
    }

    process_thread_ = std::thread(&Logger::ProcessMessage, this);
}

Logger::~Logger() {
    is_exit_.store(true);
    msg_que_cv_.notify_all();

    if(process_thread_.joinable()) {
        process_thread_.join();
    }

    if(log_file_stream_.is_open()) {
        log_file_stream_.close();
    }
}

void Logger::LoadConfig() {
    std::string CONFIG_FILE_PATH = "config/log_config.conf";
    std::ifstream config_stream(CONFIG_FILE_PATH);

    if(!config_stream.is_open()) {
        return;
    }

    std::unordered_map<std::string, LogLevel> log_level_map = {
        {"DEBUG",   LogLevel::DEBUG},
        {"NEED",    LogLevel::NEED},
        {"INFO",    LogLevel::INFO},
        {"WARN",    LogLevel::WARN},
        {"ERROR",   LogLevel::ERROR},
        {"FATAL",   LogLevel::FATAL},
    };

    std::string line;
    while(std::getline(config_stream, line)) {
       size_t pos = line.find('='); 
       if(pos == std::string::npos) {
           continue;
       }

       std::string key = line.substr(0, pos);
       std::string value = line.substr(pos + 1);

       if("LOG_LEVEL" == key) {
           auto iter = log_level_map.find(value);
           if(iter != log_level_map.end()) {
               log_level_ = iter->second;
           }
       }
       else if("LOG_FILES_MAX" == key) {
           try {
                file_num_max_ = std::stoul(value);
           } catch(...) {
               file_num_max_ = 10;
           }
       }
       else if("LOG_FILE_SIZE_MAX" == key) {
           try {
                file_size_max_ = std::stoul(value);
           } catch(...) {
               file_size_max_ = 10 * 1024 * 1024;
           }
       }
       else if("LOG_DIR_PATH" == key) {
           log_dir_path_ = value;
       }
       else if("LOG_TO_CONSOLE" == key) {
           enable_console_ = ("TRUE" == value);
       }
       else if("LOG_TO_FILE" == key) {
           enable_file_ = ("TRUE" == value);
       }
    }
}

std::string Logger::GetTimeFromFileName(const std::string& name) {
    size_t pos_start = name.find_first_of('_');
    size_t pos_end = name.find_last_of('.');
    if(pos_start == std::string::npos && pos_end == std::string::npos) {
        std::string str_time = name.substr(pos_start + 1, pos_end - pos_start - 1);
        str_time.erase(std::remove(str_time.begin(), str_time.end(), '-'), str_time.end());
        return str_time;
    }
    return "";
}

void Logger::TarLogFile() {
    auto oldest = log_file_map_.begin();
    bool has_next = std::next(oldest) != log_file_map_.end(); 
    if(fs::exists(oldest->second)) {
        if(has_next){
            auto next = std::next(oldest);
            std::string oldest_time = GetTimeFromFileName(oldest->second.filename().string());
            std::string next_time = GetTimeFromFileName(next->second.filename().string());

            if(!oldest_time.empty() && !next_time.empty()) {
                std::string tar_file_name = oldest_time + "-" + next_time + ".tar.gz";
                std::string command = "tar -czf " + log_dir_path_.string() + "/" + tar_file_name + " -C" + log_dir_path_.string() + " " + oldest->second.filename().string();
                int result = std::system(command.c_str());
                if(result != 0) {
                    std::cerr << "Failed to tar log file: " << tar_file_name << std::endl;
                }
            }
        }
        fs::remove(oldest->second);
    }
    log_file_map_.erase(oldest);
}

void Logger::SetLogMap() {
    log_file_map_.clear();

    std::regex file_pattern(R"(^(\d{13})_\d{4}-\d{2}-\d{2}-\d{2}-\d{2}-\d{2}\.log$)");

    for(const auto& entry : fs::directory_iterator(log_dir_path_)) {
        if(!fs::is_regular_file(entry)) {
            continue;
        }
        std::string file_name = entry.path().filename().string();
        std::smatch match;
        if(std::regex_match(file_name, match, file_pattern)) {
            if(fs::exists(entry.path())) {
                uint64_t ts = std::stoull(match[1].str());
                log_file_map_.emplace(ts, entry.path());
            }
        }
    }

    if(log_file_map_.empty()) {
        CreateLogFile();
    }
    else {
        while (log_file_map_.size() > file_num_max_) {
            TarLogFile();
        }
    }
}

void Logger::CreateLogFile() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    localtime_r(&now_t, &now_tm);

    std::ostringstream oss;
    oss << timestamp << "_" << std::put_time(&now_tm, "%Y-%m-%d-%H-%M-%S") << ".log";

    fs::path new_log_file_path = log_dir_path_ / oss.str();

    log_file_map_.emplace(timestamp, new_log_file_path);
    if(log_file_map_.size() > file_num_max_) {
        TarLogFile();
    }
}

void Logger::SetCurLogPath() {
    if(!log_file_map_.empty()) {
        cur_log_path_ = log_file_map_.rbegin()->second;
        if(log_file_stream_.is_open()) {
            log_file_stream_.close();
        }
        log_file_stream_.open(cur_log_path_, std::ios::out | std::ios::app);
        cur_file_size_ = fs::file_size(cur_log_path_);
    }
}

void Logger::WriteToFile(const std::string& msg) {
    constexpr int MAX_RETRIES = 3;

    bool can_write = true;

    {
        std::lock_guard<std::mutex> lock(log_file_map_mutex_);
        if(log_file_map_.empty() || msg.size() + cur_file_size_ > file_size_max_) {
            CreateLogFile();
            SetCurLogPath();
        }

        auto start = std::chrono::system_clock::now();
        while(!fs::exists(cur_log_path_)) {
            if(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count() > MAX_RETRIES) {
                can_write = false;
                break;
            }
            SetLogMap();
            SetCurLogPath();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    if(can_write) {
        log_file_stream_.write(msg.data(), msg.size());
        log_file_stream_.flush();
        cur_file_size_ += msg.size();
    }
}

void Logger::ProcessMessage() {
    while (true)
    {
        std::unique_lock<std::mutex> lock(msg_que_mutex_);
        msg_que_cv_.wait(lock, [this] {
            return !msg_que_.empty() || is_exit_.load();
        });

        if(is_exit_.load() && msg_que_.empty()) {
            break;
        }

        bool que_is_empty = msg_que_.empty();
        lock.unlock();

        while(!que_is_empty) {
            lock.lock();
            std::string msg = std::move(msg_que_.front());
            msg_que_.pop();
            que_is_empty = msg_que_.empty();
            lock.unlock();

            if(enable_console_) {
                std::cout << msg;
            }

            if(enable_file_) {
                WriteToFile(msg);
            }
        }
    }
}

void Logger::AddMsgToQue(const std::string& msg) {
    std::lock_guard<std::mutex> lock(msg_que_mutex_);
    msg_que_.push(msg);
    msg_que_cv_.notify_one();
}

Logger::LogStream::LogStream(LogLevel level, const char* file, const char* func, int line)
    : enabled_(level >= GetInstance().log_level_) {
    if(!enabled_) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto now_t = std::chrono::system_clock::to_time_t(now);

    std::tm not_tm = *std::localtime(&now_t);

    oss_ << std::put_time(&not_tm, "[%Y-%m-%d %H:%M:%S:")<< std::setfill('0') << std::setw(3) << ms.count() << "]["
    << [this, level] {
       switch(level) {
           case LogLevel::DEBUG: return "DEBUG";
           case LogLevel::NEED:  return "NEED";
           case LogLevel::INFO:  return "INFO";
           case LogLevel::WARN:  return "WARN";
           case LogLevel::ERROR: return "ERROR";
           case LogLevel::FATAL: return "FATAL";
           default: return "UNKNOWN";
       }
    }() << "]";

    const char* slash = strrchr(file, '/');
    if(!slash) {
        slash = strrchr(file, '\\');
    }
    oss_ << "[" << (slash ? slash + 1 : file) << "][" << func << "(" << line << ")] ";
}

Logger::LogStream::~LogStream() {
    if(!enabled_) {
        return;
    }

    Logger::GetInstance().AddMsgToQue(oss_.str());
}

Logger::LogStream& Logger::LogStream::operator<<(std::ostream& (*f)(std::ostream&)) {
    if(enabled_) {
        f(oss_);
    }
    return *this;
}

void Logger::LogStream::Printf(const char* fmt, ...) {
    if(!enabled_) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    oss_ << buf;
}