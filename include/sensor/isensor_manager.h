#ifndef ISENSOR_MANAGER_H
#define ISENSOR_MANAGER_H

#include "mock_sensor.h"
#include "sensor_types.h"
#include <string>

class ISensorManager {
public:
    virtual ~ISensorManager() = default;
    virtual bool start(const std::string& configPath, int refreshIntervalSeconds) = 0;
    virtual void stop() = 0;
    virtual std::optional<SensorData> getSensorDataRealTime(const std::string& id) = 0;
    virtual std::optional<SensorData> getSensorDataCached(const std::string& id) = 0;
}; 