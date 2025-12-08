#ifndef ISENSOR_MANAGER_H
#define ISENSOR_MANAGER_H

#include "sensor_types.h"
#include "config_info.h"
#include <string>
#include <optional>
#include <vector>

class ISensorManager {
public:
    virtual ~ISensorManager() = default;
    virtual void stop() = 0;
    virtual std::optional<SensorData> getSensorDataRealTime(const std::string& id) = 0; //实时数据
    virtual std::optional<SensorData> getSensorDataCached(const std::string& id) = 0; //读缓存
    virtual std::vector<SensorData> getAllSensorData() = 0; 
}; 

#endif // ISENSOR_MANAGER_H