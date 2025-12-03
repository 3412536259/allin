#pragma once

#include <string>

class PLCConnector {
public:
    virtual ~PLCConnector() = default;

    /**
     * @brief 连接到 PLC
     */
    virtual bool connect() = 0;

    /**
     * @brief 断开连接
     */
    virtual void disconnect() = 0;

    /**
     * @brief 获取 PLC 连接状态
     */
    virtual std::string getConnectionStatus() const = 0;

    /**
     * @brief 读取单个寄存器的值 (例如，用于查询设备状态)
     * @param registerAddress 寄存器地址，例如 "0x0001"
     * @return 寄存器的值（例如 "1" 或 "0"）
     */
    virtual std::string readRegister(const std::string& registerAddress) = 0;

    /**
     * @brief 写入单个寄存器的值 (例如，用于操作设备)
     * @param registerAddress 寄存器地址
     * @param value 要写入的值
     * @return 操作是否成功
     */
    virtual bool writeRegister(const std::string& registerAddress, const std::string& value) = 0;
};