#pragma once

#include "plc_connector.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

/**
 * @brief GatewayTCPConnector 实现了通过 Modbus TCP/IP 协议连接智能网关
 * * 适用于连接 Modbus 网关或直接支持 Modbus TCP 的 PLC。
 * 使用 MBAP 协议头取代 RTU 协议的 CRC 校验。
 */
class GatewayTCPConnector : public PLCConnector {
public:
    GatewayTCPConnector(const PLCConfig& config);
    ~GatewayTCPConnector() override;

    bool connect() override;
    void disconnect() override;
    std::string getConnectionStatus() const override;

    std::string readRegister(const std::string& address) override;
    bool writeRegister(const std::string& address, const std::string& value) override;

private:
    // Linux/POSIX socket 文件描述符
    int socket_fd_; 
    // Modbus TCP 事务处理标识符 (Transaction Identifier)
    std::atomic<uint16_t> next_transaction_id_; 
    // 保护 socket 读写操作的互斥锁
    std::mutex io_mutex_; 

    std::string status_ = "DISCONNECTED";

    /**
     * @brief 实际的 TCP 连接建立和配置操作。
     * @return 成功返回 true，否则返回 false。
     */
    bool openSocket();

    /**
     * @brief 关闭 TCP socket。
     */
    void closeSocket();

    /**
     * @brief 发送数据到 socket。
     * @param data 要发送的字节向量。
     * @return 实际发送的字节数。
     */
    size_t sendToSocket(const std::vector<char>& data);

    /**
     * @brief 从 socket 读取数据，带超时。
     * @param expectedMinBytes 期望读取的最小字节数。
     * @param timeout_ms 读取超时时间 (毫秒)。
     * @return 接收到的数据。
     */
    std::vector<char> readFromSocket(size_t expectedMinBytes, int timeout_ms = 2000);

    /**
     * @brief 构建完整的 Modbus TCP/IP 报文帧 (MBAP Header + PDU)。
     * @param pdu Modbus 协议数据单元 (功能码 + 数据)。
     * @param transactionId 事务 ID。
     * @param unitId 单元 ID (Modbus Slave ID)。
     * @return 完整的 Modbus TCP 报文帧。
     */
    std::vector<char> buildTCPFrame(const std::vector<char>& pdu, uint16_t transactionId, uint8_t unitId);
};