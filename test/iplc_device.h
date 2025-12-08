#pragma once

#include <string>
#include "config_info.h"

class IPLCDevice{
public:
    virtual ~IPLCDevice() = default;

    virtual std::string readStatus() = 0;   // 读取设备状态
    virtual OperateResult writeControl(const std::string& cmd) = 0; // 写入控制命令，返回操作结果
    virtual std::string getDeviceId() const = 0;
};