#pragma once

#include <string>
#include "config_info.h"
#include "plc_info.h"
#include "plc_connector.h"
#include "iplc_device.h"

/**
 * @brief PLC设备基类：封装配置和连接器引用
 */
class BasePLCDevice : public IPLCDevice {
public:
    BasePLCDevice(const PLCDeviceConfig& deviceConfig, PLCConnector* connector)
        : deviceConfig_(deviceConfig), connector_(connector) {}
    
    virtual ~BasePLCDevice() = default;
    const std::string getDeviceId() const override {
        return deviceConfig_.id;
    }
protected:
    PLCDeviceConfig deviceConfig_;
    PLCConnector* connector_;
};