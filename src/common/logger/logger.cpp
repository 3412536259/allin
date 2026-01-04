#include "logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

static std::string extractBaseName(const std::string& path) {
    // 从路径中提取目录 + 基本名（无扩展）
    size_t lastSlash = path.find_last_of('/');
    std::string dir = (lastSlash == std::string::npos) ? "." : path.substr(0, lastSlash);
    std::string filename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
    size_t dot = filename.find_last_of('.');
    std::string base = (dot == std::string::npos) ? filename : filename.substr(0, dot);
    return dir + "/" + base;
}

static std::tm parseDate(const std::string& dateStr) {
    std::tm tm{};
    tm.tm_year = std::stoi(dateStr.substr(0,4)) - 1900;
    tm.tm_mon  = std::stoi(dateStr.substr(5,2)) - 1;
    tm.tm_mday = std::stoi(dateStr.substr(8,2));
    tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
    return tm;
}

Logger::Logger() : logFile() {
    // 默认日志路径：项目根目录下的log文件夹（原名保持，实际按天产生文件）
    logPath = "./log/dingchang3576.log";
    baseLogPath = extractBaseName(logPath); // ./log/dingchang3576

    // 创建log文件夹（若不存在）
    fs::create_directories(baseLogPath.substr(0, baseLogPath.find_last_of('/')));

    // 打开当天的日志文件（追加模式）
    currentDate = getCurrentDate();
    std::string datedPath = getDatedLogPath(currentDate);
    logFile.open(datedPath, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << datedPath << std::endl;
    }

    // 启动一次清理旧日志
    cleanupOldLogs();
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance; // 静态局部变量，确保仅初始化一次
    return instance;
}

void Logger::setLogPath(const std::string& log_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile.is_open()) {
        logFile.close();
    }
    logPath = log_path;
    baseLogPath = extractBaseName(logPath);
    fs::create_directories(baseLogPath.substr(0, baseLogPath.find_last_of('/')));
    currentDate = getCurrentDate();
    std::string datedPath = getDatedLogPath(currentDate);

    // 重新打开当天日志文件
    logFile.open(datedPath, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << datedPath << std::endl;
    }

    // 运行一次清理
    cleanupOldLogs();
}

void Logger::setLogRetentionDays(int days) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (days < 0) days = 0;
    retentionDays_ = days;
}

void Logger::rotateIfNeeded() {
    std::string today = getCurrentDate();
    if (today != currentDate) {
        // 日切换：关闭旧文件，打开新文件，并清理旧日志
        if (logFile.is_open()) logFile.close();
        currentDate = today;
        std::string datedPath = getDatedLogPath(currentDate);
        logFile.open(datedPath, std::ios::out | std::ios::app);
        if (!logFile.is_open()) {
            std::cerr << "Failed to open log file: " << datedPath << std::endl;
        }
        cleanupOldLogs();
    }
}

void Logger::cleanupOldLogs() {
    try {
        if (retentionDays_ <= 0) return; // 非正值表示不清理
        std::string dir = baseLogPath.substr(0, baseLogPath.find_last_of('/'));
        std::string baseName = baseLogPath.substr(baseLogPath.find_last_of('/') + 1);
        // 匹配模式： baseName-YYYY-MM-DD.log
        std::regex pattern(baseName + "-(\\d{4}-\\d{2}-\\d{2})\\.log$");

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            std::smatch m;
            std::string fname = entry.path().filename().string();
            if (std::regex_search(fname, m, pattern)) {
                std::string dateStr = m[1].str();
                std::tm tm = parseDate(dateStr);
                std::time_t fileTime = std::mktime(&tm);
                std::time_t now = std::time(nullptr);
                double days = std::difftime(now, fileTime) / (60 * 60 * 24);
                if (days > retentionDays_) {
                    // 删除旧文件
                    std::error_code ec;
                    fs::remove(entry.path(), ec);
                    if (ec) {
                        std::cerr << "Failed to remove old log file " << entry.path() << ": " << ec.message() << std::endl;
                    } else {
                        std::cout << "Removed old log file: " << entry.path() << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in cleanupOldLogs: " << e.what() << std::endl;
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    rotateIfNeeded();

    std::string timeStr = getCurrentTime();
    std::string levelStr = logLevelToString(level);
    std::string logMsg = "[" + timeStr + "] [" + levelStr + "] " + message + "\n";

    // 输出到控制台
    std::cout << logMsg;
    // 写入日志文件
    if (logFile.is_open()) {
        logFile << logMsg;
        logFile.flush(); // 强制刷新缓冲区，确保日志实时写入
    }
}

std::string Logger::getCurrentTime() const {
    std::time_t now = std::time(nullptr);
    std::tm* tmNow = std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(tmNow, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Logger::getCurrentDate() const {
    std::time_t now = std::time(nullptr);
    std::tm* tmNow = std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(tmNow, "%Y-%m-%d");
    return ss.str();
}

std::string Logger::getDatedLogPath(const std::string& date) const {
    // baseLogPath: ./log/dingchang3576
    return baseLogPath + "-" + date + ".log";
}

std::string Logger::logLevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}