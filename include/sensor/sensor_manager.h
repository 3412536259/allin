#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "isensor_manager.h"
#include "isensor.h"
#include "config_parser.h"
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

class SensorManager : public ISensorManager {
public:
    SensorManager();
    ~SensorManager() override;

    void stop() override;
    std::optional<SensorData> getSensorDataRealTime(const std::string& id) override;
    std::optional<SensorData> getSensorDataCached(const std::string& id) override;
    std::vector<SensorData> getAllSensorData() override;
    bool refreshSensor(const std::string& id);
    void refreshAllSensors();

private:
    std::unordered_map<std::string, std::unique_ptr<ISensor>> sensors_;
    std::unordered_map<std::string, SensorData> cache_;
    mutable std::mutex mu_;

    std::thread refreshThread_;
    std::atomic<bool> running_{false};
    int refreshIntervalSeconds_ = 5;

    void refreshLoop();
};

#endif // SENSOR_MANAGER_H