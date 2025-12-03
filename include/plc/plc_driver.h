#pragma once
#include <string>
#include <mutex>
#include "config_info.h"
#include "plc_info.h"

class PLCDriver{
public:
    virtual ~PLCDriver() = default;

    // 启动通信
    virtual bool start() = 0;

    // 停止通信
    virtual void stop() = 0;
    
    // 读取寄存器
    virtual bool readRegister(const std::string& address, int& outData) = 0;

    // 写入寄存器
    virtual bool writeRegister(const std::string& address, int inData) = 0;

    virtual PLCState getStatus() = 0;

protected:
    std::mutex commMutex_;
};