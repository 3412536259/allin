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

// PLC设备配置结构体
struct PLCConfig {
    std::string id;
    std::string type;        // 设备类型，如"SolenoidValve", "Mock"等
    std::string serialPort;     // 串口号
    int baudrate;               // 波特率

    int slaveId;                // Modbus地址 
    int regValve;        // 控制阀门的寄存器地址
};