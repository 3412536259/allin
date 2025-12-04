#pragma once

#include "config_info.h"
#include "plc_connector.h"
#include <string>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <chrono>

// 辅助函数声明（仅暴露必要接口，实现放在.cpp）
unsigned short calculate_crc16(const std::vector<char>& data);
std::vector<char> HexStringToBytes(const std::string& hexFrame);
std::string BytesToHexString(const std::vector<char>& data);

/**
 * @brief SerialPLCConnector 实体类
 * 实现了基于 Modbus 报文的真实 termios 串口操作流程。
 */
class SerialPLCConnector : public PLCConnector {
public:
    explicit SerialPLCConnector(const PLCConfig& config);
    ~SerialPLCConnector() override;

    bool connect() override;
    void disconnect() override;
    std::string getConnectionStatus() const override;
    std::string readRegister(const std::string& address) override;
    bool writeRegister(const std::string& address, const std::string& value) override;

private:
    int serialHandle_ = 0; // 串口句柄 (文件描述符)
    std::string status_ = "DISCONNECTED";

    inline speed_t getBaudRateConstant(int baudRate);
    bool openSerialPort();
    void closeSerialPort();
    size_t writeToSerial(const std::vector<char>& data);
    std::vector<char> readFromSerial(size_t expectedMinBytes, int timeout_ms = 2000);
};