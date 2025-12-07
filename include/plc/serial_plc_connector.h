#pragma once

#include "config_info.h"
#include "plc_common_utils.h"
#include "plc_connector.h"
#include <string>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <chrono>

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

    std::vector<char> buildModbusFrame(uint8_t funcCode, const std::vector<char>& data) const;
    std::vector<char> exchangeFrame(const std::vector<char>& txFrame, size_t expectedMinBytes, int timeout_ms);

    bool performHealthCheck(); // 检查连接，发送并校验特定的健康报文
    bool validateResponse(const std::vector<char>& response, uint8_t expectedFuncCode, size_t expectedMinLength) const; //校验相应结构

    std::vector<char> addressToBytes(const std::string& registerAddress) const; // 地址和数据转换辅助函数
};