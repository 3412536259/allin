#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <string>

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
};

#endif // SENSOR_TYPES_H
