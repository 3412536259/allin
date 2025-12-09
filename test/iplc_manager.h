#pragma once

#include <string>
#include <vector>
#include "plc_info"

class IPLCManager{
public:
    virtual ~IPLCManager() = default;

    virtual PLCInfo getStatus(const std::string& deviceId) = 0;
    virtual std::vector<PLCInfo> getAllStatus() = 0;
    virtual OperateResult operate(const std::string& deviceId, const std::string& cmd) = 0;
};