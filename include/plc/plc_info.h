#pragma once

#include <string>

// PLC设备的在线/不在线状态
enum class PLCState{
    OFFLINE = 0,
    ONLINE = 1,
};

// PLC操作结果
enum class OperateResult{
    SUCCESS,
    FAILED,
    TIMEOUT,
};

// PLC设备信息结构体
struct PLCInfo{
    std::string id;
    PLCState state;
    std::string type; 
};