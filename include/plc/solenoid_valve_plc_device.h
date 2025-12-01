#pragma once

#include "plc_info.h"
#include <string>
#include <vector>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include "iplc_device.h"

class SolenoidValvePLCDevice : public PLCDevice{
public:
    SolenoidValvePLCDevice(const PLCConfig& cfg);
    ~SolenoidValvePLCDevice();

    PLCState queryStatus() override;
    std::string getId() const override;
    std::string getType() const override;

    OperateResult operate(const std::string& cmd) override;

private:
    // 读写寄存器函数
    OperateResult doWriteValue(bool open);

    // 报文处理
    std::vector<uint8_t> buildWriteCoilRequest(bool value);
    bool validateResponse(const std::vector<uint8_t>& req, const std::vector<uint8_t>& resp);
    static uint16_t crc16(const uint8_t* data, int len);

    // 串口通讯
    bool sendFrame(const std::vector<uint8_t>& frame);
    OperateResult recvFrame(std::vector<uint8_t>& out, int timeoutMs);

    // 工具
    void openSerial();
    void printHex(const uint8_t* data, int len);

    
private:
    PLCConfig cfg_;
    int serialFd_ = -1; // 串口标识符
    PLCState state_ = PLCState::OFFLINE;
};