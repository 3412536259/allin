#pragma once

#include <string>
#include <vector>

// 设备信息结构体
struct PLCDeviceStatus{
    std::string id;
    std::string name;
    std::string registerAddress;
    std::string status;
    std::string lastUpdateTime;
};



// PLC自身信息结构体
struct PLCInfo{
    std::string plcId;
    std::string name;
    std::string connectionStatus;
    std::string lastUpdateTime;
    std::vector<PLCDeviceStatus> deviceStatuses;
};


struct OperateResult{
    bool success;
    std::string message;
};

struct PLCList{
    std::vector<PLCInfo> plcList;
};