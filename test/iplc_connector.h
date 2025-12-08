#pragma once

#include <string>
#include "config_info.h"

class IPLCConnector{
public:
    virtual ~IPLCConnector() = default;
    
    virtual bool link() = 0;    //连接到PLC
    virtual void disconnect() = 0;  //断开连接
    virtual std::string getConnectionStatus() const = 0;    //获取PLC的连接状态
    virtual std::string readRegister(const std::string& registerAddress) = 0;   //读寄存器的值
    virtual std::string writeRegister(const std::string& registerAddress, const std::string& value) = 0;    //写寄存器的值
};