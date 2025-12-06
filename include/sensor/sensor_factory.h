// sensor_factory.h
#ifndef SENSOR_FACTORY_H
#define SENSOR_FACTORY_H

#include "isensor.h"
#include "config_info.h"
#include "modbus_sensor.h"
//#include "simulated_sensor.h"
#include "gpio_sensor.h"
#include "custom_sensor.h"  // ← 新增
#include <memory>
#include <string>

inline std::unique_ptr<ISensor> createSensor(const SensorConfig& config) {
    std::string type = config.type;
    for (auto& c : type) c = static_cast<char>(tolower(c));

    if (type == "modbus" || type.empty()) {
        return std::make_unique<ModbusSensor>(config);
    }
    if (type == "gpio") {
        return std::make_unique<GPIOSensor>(config);
    }
    if (type == "custom" || type == "proprietary") {
        return std::make_unique<CustomProtocolSensor>(config);
    }
    // if (type == "mock" || type == "simulated" || type == "fake") {
    //     return std::make_unique<SimulatedSensor>(config);
    // }

    // fallback
    return std::make_unique<ModbusSensor>(config);
}

#endif // SENSOR_FACTORY_H