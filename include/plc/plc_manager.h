#pragma once

#include "config_info.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "iplc_device.h"
#include "plc_device_factory.h"


class PLCManager{
public:
    PLCManager();
    ~PLCManager();

    bool start();
    PLCInfo getStatus(const std::string& deviceId);
    std::vector<PLCInfo> getAllStatus();
    OperateResult operate(const std::string& deviceId, const std::string& cmd);

private:
    bool loadConfig();
    bool registerDevices();

private:
    struct PLCRuntimeState{
        PLCState state = PLCState::OFFLINE;
        std::string type;
    };

    // 运行态
    std::unordered_map<std::string, PLCRuntimeState> deviceStateTable_;
    // 设备对象
    std::vector<PLCDevice*> devices_;
    // 解析后的配置
    std::vector<PLCConfig> deviceConfigs_;
    std::mutex plcMutex_;
};