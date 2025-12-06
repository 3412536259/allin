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
#include "config_info.h"
#include "config_parser.h"
#include "isensor_manager.h"
#include "sensor_factory.h"
#include "isensor.h"

class SensorManager : public ISensorManager {
public:
    SensorManager();
    ~SensorManager();

    void stop();
    std::optional<SensorData> getSensorDataRealTime(const std::string& id);
    std::optional<SensorData> getSensorDataCached(const std::string& id);
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