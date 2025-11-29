#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "plc_info.h"

class PLCDevice{
public:
    virtual ~PLCDevice() = default;
    virtual PLCState queryStatus() = 0;
    virtual std::string getId() const = 0;
    virtual bool operate(const std::string& cmd) = 0;
};

class PLCManager{
public:
    PLCManager();
    ~PLCManager();

    bool start();
    PLCInfo getStatus(const std::string& deviceId);
    std::vector<PLCInfo> getAllStatus();
    bool operate(const std::string& deviceId, const std::string& cmd);

private:
    bool loadConfig();
    bool registerDevices();

private:
    struct PLCRuntimeState{
        PLCState state = PLCState::OFFLINE;
    };

    // 运行态
    std::unordered_map<std::string, PLCRuntimeState> deviceStateTable_;
    // 设备对象
    std::vector<PLCDevice*> devices_;
    std::mutex plcMutex_;
};