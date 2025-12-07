#pragma once

#include <string>
#include "config_info.h"
#include "plc_info.h"
#include "plc_connector.h"
#include "base_plc_device.h"

class SolenoidValvePLCDevice : public BasePLCDevice {
public:
    SolenoidValvePLCDevice(const PLCDeviceConfig& deviceConfig, PLCConnector* connector)
        : BasePLCDevice(deviceConfig, connector) {}
    
    std::string readStatus() override {
        std::string value = connector_->readRegister(deviceConfig_.registerAddress);
        if(value == "1") return "ON";
        if(value == "0") return "OFF";
        return "UNKNOWN (" + value + ")";
    }
    OperateResult writeControl(const std::string& cmd) override {
        OperateResult result;
        std::string value;
        if(cmd == "ON" || cmd == "1"){
            value = "1"; //打开
        } else if(cmd == "OFF" || cmd == "0"){
            value = "0"; //关闭
        } else{
            result.success = false;
            result.message = "Invalid command: " + cmd + ". Must be ON or OFF.";
            return result;
        }
        bool success = connector_->writeRegister(deviceConfig_.registerAddress, value);
        result.success = success;
        if(success){
            result.message = "Operated successfully: " + deviceConfig_.id + " set to " + cmd;
        } else{
            result.message = "Failed to operate: " + deviceConfig_.id + " set to " + cmd;
        }
        return result;
    }
};