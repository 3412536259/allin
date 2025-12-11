#pragma once

#include <string>
#include <vector>
#include "plc_info.h"


class IPLCManager{
public:
    virtual ~IPLCManager() = default;

    // PLC本体和下挂设备状态查询
    virtual PLCInfo getStatus(const std::string& deviceId) = 0;
    virtual PLCList getAllStatus() = 0;
    // 操作PLC下挂设备
    virtual OperateResult operate(const std::string& deviceId, const std::string& cmd) = 0;
};
