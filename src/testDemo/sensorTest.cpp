#include <iostream>
#include "sensor_manager.h"
#include "sensor_types.h" // 确保 SensorStatus 可见

// 辅助函数
std::string toString(SensorStatus s) {
    switch (s) {
        case SensorStatus::NORMAL:   return "NORMAL";
        case SensorStatus::ABNORMAL: return "ABNORMAL";
        case SensorStatus::OFFLINE:  return "OFFLINE";
        default: return "UNKNOWN";
    }
}

int main() {
    ConfigParser::getInstance().loadFromFile("../../include/common/config/config.json");
    SensorManager mgr;

    std::string id = "sensor_001";

    // ✅ 正确用法：接收 optional 并检查
    if (auto data = mgr.getSensorDataRealTime(id)) {
        std::cout << "ID: " << data->id << "\n"
                  << "Temp: " << data->temperature << "°C\n"
                  << "Humi: " << data->humidity << "%\n"
                  << "Status: " << toString(data->status) << "\n";
    } else {
        std::cerr << "Failed to read sensor '" << id << "' in real-time.\n";
    }

    mgr.stop();
    return 0;
}