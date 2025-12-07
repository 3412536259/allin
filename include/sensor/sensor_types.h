#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <string>
#include <chrono>   // 新增：std::chrono 依赖
#include <ctime>    // 新增：std::localtime、tm 依赖
#include <sstream>  // 新增：std::stringstream 依赖
#include <iomanip>  // 新增：std::put_time 依赖
#include <iostream> // 新增：std::cerr/std::cout 基础依赖

// 传感器状态与类型（独立于 config_info.h）
enum class SensorStatus {
    NORMAL = 0,
    ABNORMAL = -1,
    OFFLINE = 1
};

// 简单转换（用于日志/打印）
inline std::string to_string(SensorStatus s) {
    switch (s) {
        case SensorStatus::NORMAL: return "NORMAL";
        case SensorStatus::ABNORMAL: return "ABNORMAL";
        case SensorStatus::OFFLINE: return "OFFLINE";
        default: return "UNKNOWN";
    }
}

// 缓存数据结构（SensorManager 用）
struct SensorData {
    std::string id;
    float temperature = 0.0f;
    float humidity = 0.0f;
    float value = 0.0f; // 其他主值
    SensorStatus status = SensorStatus::OFFLINE;
    std::string lastUpdateTime;
};

inline std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};

    std::tm* result = std::localtime(&in_time_t);
    if (result) {
        bt = *result;
    } else {
        return "Time Error"; 
    }

    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

#endif // SENSOR_TYPES_H
