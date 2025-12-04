#pragma once

#include <string>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include "config_info.h"
#include "plc_connector.h"

class MockPLCConnector : public PLCConnector {
public:
    explicit MockPLCConnector(const PLCConfig& config) : PLCConnector(config) {}

    bool connect() override {
        // 模拟连接逻辑
        std::cout << "[Mock] Connecting to PLC: " << config_.plcId << "..." << std::endl;
        status_ = "CONNECTED";
        return true;
    }

    void disconnect() override {
        // 模拟断开连接逻辑
        std::cout << "[Mock] Disconnecting from PLC: " << config_.plcId << "..." << std::endl;
        status_ = "DISCONNECTED";
    }

    std::string getConnectionStatus() const override {
        return status_;
    }

    std::string readRegister(const std::string& registerAddress) override {
        // 模拟读取寄存器值
        // 假设偶数地址返回 "1" (ON)，奇数地址返回 "0" (OFF)
        if (registerAddress == "0x0001") return "1"; 
        if (registerAddress == "0x0002") return "0"; 
        return "UNKNOWN";
    }

    bool writeRegister(const std::string& registerAddress, const std::string& value) override {
        // 模拟写入寄存器
        std::cout << "[Mock] Writing to PLC " << config_.plcId 
                  << " register " << registerAddress << " with value " << value << std::endl;
        return true; // 模拟成功
    }

private:
    std::string status_ = "DISCONNECTED";
    std::string mockRegisterValue_;
};