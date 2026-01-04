#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <ctime>
#include <mutex>

enum class LogLevel{
    INFO,
    WARNING,
    ERROR
};

class Logger{
public:    
    //获取单例实例
    static Logger& getInstance();
    //禁止拷贝构造和赋值操作
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    //设置日志文件路径（会按天拆分，例如: <name>-YYYY-MM-DD.log）
    void setLogPath(const std::string& log_path);
    // 设置保留天数（默认 7 天）
    void setLogRetentionDays(int days);
    //写入日志
    void log(LogLevel level, const std::string& message);

private:
    Logger();
    ~Logger();

    std::string getCurrentTime() const;
    std::string getCurrentDate() const;

    std::string logLevelToString(LogLevel level) const;

    // internal helpers
    void rotateIfNeeded();
    void cleanupOldLogs();
    std::string getDatedLogPath(const std::string& date) const;

    std::ofstream logFile;
    std::string logPath;       // 原始配置路径（包含文件名）
    std::string baseLogPath;   // 目录 + 基础文件名（无日期与扩展名）
    std::string currentDate;   // 当前打开的日志对应日期 YYYY-MM-DD
    int retentionDays_{7};     // 默认保留天数
    std::mutex mutex_;         // 线程安全
};

// 日志宏定义（简化日志调用）
#define LOG_INFO(msg) Logger::getInstance().log(LogLevel::INFO, msg)
#define LOG_WARNING(msg) Logger::getInstance().log(LogLevel::WARNING, msg)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::ERROR, msg)


#endif