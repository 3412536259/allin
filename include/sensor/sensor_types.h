#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <string>
#include <chrono>   
#include <ctime>    
#include <sstream>  
#include <iomanip>  
#include <iostream>
#include <vector>
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
    std::string type;  // 新增：记录传感器类型（modbus/gpio/custom）
    float temperature = 0.0f;
    float humidity = 0.0f;
    float value = 0.0f; // 其他主值（GPIO电平/Custom自定义值）
    SensorStatus status = SensorStatus::OFFLINE;
    std::string lastUpdateTime;
};

struct SensorList{
    std::vector<SensorData> sensors;
};

// 辅助函数：按类型打印传感器数据
inline void printSensorData(const SensorData& data) {
    std::cout << "Sensor ID: " << data.id << ", ";
    
    // 按传感器类型打印不同字段
    if (data.type == "modbus") {
        std::cout << "Temperature: " << data.temperature << "°C, "
                  << "Humidity: " << data.humidity << "%, ";
    } else if (data.type == "gpio") {
        std::cout << "GPIO Value: " << data.value << ", ";
    } else if (data.type == "custom") {
        std::cout << "Custom Value: " << data.value << ", ";
    }
    
    std::cout << "Status: " << to_string(data.status)
              << ", Last Update: " << data.lastUpdateTime << std::endl;
}

inline std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm bt{}; // 替换 std::tm 为全局 tm，适配嵌入式

    struct tm* result = localtime(&in_time_t); // 去掉 std:: 前缀
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