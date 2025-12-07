#include "config_parser.h"
#include "sensor_manager.h"
#include "config_info.h"
#include "sensor_types.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    // 假设配置路径正确，并加载配置
    ConfigParser::getInstance().loadFromFile("../../include/common/config/config.json");
    
    // 初始化传感器管理器
    SensorManager manager;

    // 简单的检查传感器是否加载成功
    if (manager.getAllSensorData().empty()) {
        std::cerr << "Manager initialization failed or no sensors found. Exiting application.\n";
        return -1;
    }

    // 持续读取传感器数据
    std::cout << "\n--- 持续读取传感器数据 ---\n";
    std::vector<SensorData> allSensors = manager.getAllSensorData();
    for (const auto& sensor : allSensors) {
        std::cout << "Sensor ID: " << sensor.id 
                  << ", Temperature: " << sensor.temperature 
                  << "°C, Humidity: " << sensor.humidity 
                  << "%, Status: " << to_string(sensor.status)
                  << ", Last Update: " << sensor.lastUpdateTime << std::endl;
    }

    // 测试实时读取特定传感器
    std::cout << "\n--- 实时读取传感器数据 ---\n";
    if (!allSensors.empty()) {
        std::string testId = allSensors[0].id;
        std::optional<SensorData> realTimeData = manager.getSensorDataRealTime(testId);
        if (realTimeData) {
            std::cout << "Real-time data for sensor " << testId 
                      << ": Temp=" << realTimeData->temperature 
                      << "°C, Humidity=" << realTimeData->humidity 
                      << "%, Status=" << to_string(realTimeData->status) << std::endl;
        }
    }

    // 等待一段时间观察定时刷新
    std::cout << "\n--- 等待定时刷新... ---\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 最终状态
    std::cout << "\n--- 最终传感器状态 ---\n";
    allSensors = manager.getAllSensorData();
    for (const auto& sensor : allSensors) {
        std::cout << "Sensor ID: " << sensor.id 
                  << ", Temperature: " << sensor.temperature 
                  << "°C, Humidity: " << sensor.humidity 
                  << "%, Status: " << to_string(sensor.status)
                  << ", Last Update: " << sensor.lastUpdateTime << std::endl;
    }

    std::cout << "\n程序结束，传感器管理器将自动停止刷新线程并清理资源。\n";

    return 0;
}