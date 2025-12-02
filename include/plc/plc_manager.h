#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "config_info.h"
#include "plc_info.h"
#include "iplc_manager.h"
#include "iplc_device.h"
#include "plc_device_factory.h"


class PLCManager : public IPLCManager{
public:
    PLCManager();
    ~PLCManager() override;

    bool start() override;
    PLCInfo getStatus(const std::string& deviceId) override;
    std::vector<PLCInfo> getAllStatus() override;
    OperateResult operate(const std::string& deviceId, const std::string& cmd) override;

private:
    bool loadConfig();
    bool registerDevices();

private:
    struct PLCRuntimeState{
        PLCState state = PLCState::OFFLINE;
        std::string type;
    };

    // 完整配置
    DeviceConfigRoot rootConfig_;
    // 运行态
    std::unordered_map<std::string, PLCRuntimeState> deviceStateTable_;
    // 设备对象
    std::vector<PLCDevice*> devices_;
    // plc的配置
    std::vector<PLCDeviceConfig> plcConfigs_;
    std::mutex plcMutex_;
};