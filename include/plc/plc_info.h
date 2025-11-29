#pragma once

#include <string>
#include "plc_manager.h"

// PLC设备的在线/不在线状态
enum class PLCState{
    OFFLINE = 0,
    ONLINE = 1,
};

// PLC设备信息结构体
struct PLCInfo{
    std::string id;
    PLCState state;
};