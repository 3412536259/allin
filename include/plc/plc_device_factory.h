#pragma once

#include "plc_info.h"
#include <string>
#include "mock_plc_device.h"
#include "solenoid_valve_plc_device.h"


class PLCDeviceFactory{
public:
    static PLCDevice* createDevice(const PLCConfig& cfg){
        if(cfg.type == "SolenoidValve"){
            return new SolenoidValvePLCDevice(cfg);
        }
        else if(cfg.type == "Mock"){
            return new MockPLCDevice(cfg.id);
        }
        return nullptr;
    }
};