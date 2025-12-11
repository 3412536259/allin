#pragma once

#include "config_info.h"
#include "device_info.h"
#include <string>

class ICarControlManager {
public:
    virtual ~ICarControlManager() = default;
    // initialize manager from global config (optional)
    virtual bool initFromConfig() = 0;
    // initialize single device from CarControlConfig
    virtual bool initWithConfig(const CarControlConfig& cc) = 0;
    // operate a specific car control device synchronously
    virtual CarControlResult operate(const std::string& id, int motor1, int motor2) = 0;
    // shutdown manager and close devices
    virtual void shutdown() = 0;
};

