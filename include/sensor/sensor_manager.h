#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <optional>
#include "sensor_types.h"
#include "mock_sensor.h"
#include "config_info.h"
#include "config_parser.h"
#include "isensor_manager.h"

class SensorManager : public ISensorManager {
public:
    SensorManager();
    ~SensorManager();

    // 禁止拷贝
    SensorManager(const SensorManager&) = delete;
    SensorManager& operator=(const SensorManager&) = delete;

    // 启动：加载配置并启动后台刷新线程（refreshIntervalSeconds 默认为5秒）
    bool start(const std::string& configPath = "config.json", int refreshIntervalSeconds = 5);

    // 停止并清理
    void stop();

    // 实时读取（每次访问会访问硬件）
    std::optional<SensorData> getSensorDataRealTime(const std::string& id);

    // 读取缓存（不访问硬件）
    std::optional<SensorData> getSensorDataCached(const std::string& id);

    // 同步刷新单个/全部传感器
    bool refreshSensor(const std::string& id);
    void refreshAllSensors();

private:
    bool loadSensors(const std::string& configPath);

    std::unordered_map<std::string, std::unique_ptr<MockSensor>> sensors_;
    std::unordered_map<std::string, SensorData> cache_;
    std::mutex mu_;

    std::thread refreshThread_;
    std::atomic<bool> running_;
    int refreshIntervalSeconds_ = 5;

    void refreshLoop();
};

#endif // SENSOR_MANAGER_H
