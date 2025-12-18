#include "gateway_tcp_connector.h"
#include "plc_common_utils.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "logger.h"

// -------------------------------------------------------------------
// GatewayTCPConnector 类实现
// -------------------------------------------------------------------

GatewayTCPConnector::GatewayTCPConnector(const PLCConfig& config)
    : PLCConnector(config), socket_fd_(-1), next_transaction_id_(1) {
    // 确保 Unit ID 存在，Modbus RTU Slave ID 0x01
    // 对于网关，通常使用 Slave ID 0x01
    if (config_.plcId.empty()) { 
        std::cerr << "[GatewayTCP] WARNING: PLC ID is empty, connection might fail.\n";
        LOG_WARNING("[GatewayTCP] PLC ID is empty, connection might fail.");

    }
}

GatewayTCPConnector::~GatewayTCPConnector() {
    disconnect();
}

/**
 * @brief 构造 Modbus TCP/IP 报文帧。
 * 报文结构: TID(2) + PID(2) + Length(2) + Unit ID(1) + PDU(n)
 */
std::vector<char> GatewayTCPConnector::buildTCPFrame(const std::vector<char>& pdu, uint16_t transactionId, uint8_t unitId) {
    std::vector<char> frame;
    
    // 1. 事务处理标识符 (TID, 2 bytes, Big Endian)
    frame.push_back((char)(transactionId >> 8));
    frame.push_back((char)(transactionId & 0xFF));
    
    // 2. 协议标识符 (PID, 2 bytes, always 0x0000)
    frame.push_back(0x00);
    frame.push_back(0x00);
    
    // 3. 长度字段 (Length, 2 bytes, PDU 长度 + Unit ID 1 byte, Big Endian)
    uint16_t length = pdu.size() + 1; 
    frame.push_back((char)(length >> 8));
    frame.push_back((char)(length & 0xFF));
    
    // 4. 单元标识符 (Unit ID, 1 byte, 对应 Modbus RTU 的 Slave ID)
    frame.push_back(unitId);
    
    // 5. PDU (功能码 + 数据)
    frame.insert(frame.end(), pdu.begin(), pdu.end());
    
    return frame;
}

/**
 * @brief 真正的 TCP 连接过程：建立 socket 并连接到网关。
 */
bool GatewayTCPConnector::connect() {
    std::lock_guard<std::mutex> lock(io_mutex_);
    if (socket_fd_ != -1) {
        std::cout << "[GatewayTCP:" << config_.plcId << "] Already connected." << std::endl;
        LOG_INFO("[GatewayTCP] Already connected.");
        return true;
    }

    // 1. 打开 Socket 并连接 (I/O 逻辑)
    if (!openSocket()) {
        status_ = "PORT_ERROR";
        return false;
    }

    // 2. 执行 Modbus TCP 级别的连接检查 (业务逻辑)，使用配置的 Unit ID
    // 假设 config_.slaveId 包含了 PLC 的 Unit ID
    if (!performHealthCheck(config_.slaveId)) { 
        // HealthCheck 失败时，必须关闭 Socket
        closeSocket(); 
        status_ = "DISCONNECTED";
        return false;
    }
    
    // 3. 连接成功
    status_ = "CONNECTED";
    std::cout << "[GatewayTCP:" << config_.plcId << "] Connection established (Gateway Online).\n";
    LOG_INFO("[GatewayTCP] Connection established (Gateway Online).");
    return true;
}

/**
 * @brief 关闭 Socket。
 */
void GatewayTCPConnector::disconnect() {
    std::lock_guard<std::mutex> lock(io_mutex_);
    closeSocket();
    status_ = "DISCONNECTED";
}

std::string GatewayTCPConnector::getConnectionStatus() const {
    return status_;
}

/**
 * @brief 读取下挂设备状态（Modbus TCP）。
 */
std::string GatewayTCPConnector::readRegister(const std::string& address) {
    std::lock_guard<std::mutex> lock(io_mutex_);

    if (status_ != "CONNECTED" || socket_fd_ == -1) {
        std::cout << "[GatewayTCP:" << config_.plcId << "] ERROR: Cannot read, Gateway is DISCONNECTED." << std::endl;
        LOG_ERROR("[GatewayTCP] Cannot read, Gateway is DISCONNECTED.");
        return "ERROR";
    }
    
    const uint8_t FUNC_READ_COILS = 0x01;
    const size_t MIN_RESPONSE_LENGTH = 10; // MBAP(7) + FC(1) + ByteCount(1) + Data(1)

    // 1. 构造 PDU (功能码 01: Read Coils)
    std::vector<char> addrBytes;
    try {
        addrBytes = addressToBytes(address);
    } catch (const std::invalid_argument& e) {
        std::cerr << "[GatewayTCP:" << config_.plcId << "] ERROR: " << e.what() << std::endl;
        LOG_ERROR("[GatewayTCP] " + std::string(e.what()));
        return "ERROR";
    }

    // PDU structure: Func(1) Addr_Hi(1) Addr_Lo(1) Count_Hi(1) Count_Lo(1)
    std::vector<char> readPDU = { 
        FUNC_READ_COILS, 
        addrBytes[0], addrBytes[1], 
        0x00, 0x01 // Read 1 coil
    };

    uint16_t tid = next_transaction_id_.fetch_add(1);
    uint8_t unitId = config_.slaveId; 
    std::vector<char> readFrame = buildTCPFrame(readPDU, tid, unitId);

    std::cout << "[GatewayTCP:" << config_.plcId << "] Reading Device Status on Address " << address << ":\n";
    LOG_INFO("[GatewayTCP] Reading Device Status on Address " + address);
    std::cout << "  -> TX Sent: " << BytesToHexString(readFrame) << " (" << readFrame.size() << " bytes)\n";
    LOG_INFO("[GatewayTCP] TX Sent: " + BytesToHexString(readFrame));
    
    // 2. 帧交换
    if (sendToSocket(readFrame) == 0) return "ERROR";
    
    std::vector<char> response = readFromSocket(MIN_RESPONSE_LENGTH); 

    // 3. 校验和解析
    if (!validateResponse(response, tid, FUNC_READ_COILS, MIN_RESPONSE_LENGTH)) {
        return "ERROR";
    }

    // 响应帧的第 9 字节是数据位
    std::string status = (response[9] & 0x01) ? "1" : "0";
    std::cout << "[GatewayTCP:" << config_.plcId << "] Read Success. (Status: " << status << ")\n";
    LOG_INFO("[GatewayTCP] Read Success. (Status: " + status + ")");
    return status;
}

/**
 * @brief 写入寄存器的值（Modbus TCP）。
 */
bool GatewayTCPConnector::writeRegister(const std::string& address, const std::string& value) {
    std::lock_guard<std::mutex> lock(io_mutex_);

    if (status_ != "CONNECTED" || socket_fd_ == -1) {
        std::cout << "[GatewayTCP:" << config_.plcId << "] ERROR: Write failed, Gateway is DISCONNECTED." << std::endl;
        LOG_ERROR("[GatewayTCP] Write failed, Gateway is DISCONNECTED.");
        return false;
    }

    const uint8_t FUNC_WRITE_SINGLE_COIL = 0x05;
    const size_t EXPECTED_ECHO_LENGTH = 12; // MBAP(7) + PDU(5)

    // 确定写入数据值 (FF00 或 0000)
    std::vector<char> dataValue;
    if (value == "1" || value == "ON") {
        dataValue = { (char)0xFF, (char)0x00 }; 
    } else if (value == "0" || value == "OFF") {
        dataValue = { (char)0x00, (char)0x00 };
    } else {
        std::cerr << "[GatewayTCP:" << config_.plcId << "] Invalid write value: " << value << std::endl;
        LOG_ERROR("[GatewayTCP] Invalid write value: " + value);
        return false;
    }

    // 1. 构造 PDU (功能码 05: Write Single Coil)
    std::vector<char> addrBytes;
    try {
        addrBytes = addressToBytes(address);
    } catch (const std::invalid_argument& e) {
        std::cerr << "[GatewayTCP:" << config_.plcId << "] ERROR: " << e.what() << std::endl;
        LOG_ERROR("[GatewayTCP] " + std::string(e.what()));
        return false;
    }

    // PDU structure: Func(1) Addr_Hi(1) Addr_Lo(1) Data_Hi(1) Data_Lo(1)
    std::vector<char> writePDU = { 
        FUNC_WRITE_SINGLE_COIL, 
        addrBytes[0], addrBytes[1], 
        dataValue[0], dataValue[1] 
    };
    
    uint16_t tid = next_transaction_id_.fetch_add(1);
    uint8_t unitId = config_.slaveId; 
    std::vector<char> writeFrame = buildTCPFrame(writePDU, tid, unitId);

    std::cout << "[GatewayTCP:" << config_.plcId << "] Writing Device Status on Address " << address << " with value " << value << ":\n";
    LOG_INFO("[GatewayTCP] Writing Device Status on Address " + address + " with value " + value);
    std::cout << "  -> TX Sent: " << BytesToHexString(writeFrame) << " (" << writeFrame.size() << " bytes)\n";
    LOG_INFO("[GatewayTCP] TX Sent: " + BytesToHexString(writeFrame));
    
    // 2. 帧交换
    if (sendToSocket(writeFrame) == 0) return false;
    
    std::vector<char> response = readFromSocket(EXPECTED_ECHO_LENGTH); 

    // 3. 校验 Echo Frame
    if (!validateResponse(response, tid, FUNC_WRITE_SINGLE_COIL, EXPECTED_ECHO_LENGTH)) {
        return false;
    }

    // 4. 进一步校验 PDU 部分是否与发送帧匹配 (写操作特有)
    // 比较发送帧的 PDU 部分 (从第 7 字节开始)
    bool isEchoMatch = (response.size() == writeFrame.size() && 
                        std::equal(response.begin() + 7, response.end(), writeFrame.begin() + 7));

    if (!isEchoMatch) {
        std::cerr << "[GatewayTCP:" << config_.plcId << "] ERROR: Write failed. PDU Echo frame mismatch.\n";
        LOG_ERROR("[GatewayTCP] Write failed. PDU Echo frame mismatch.");
        return false;
    }
    
    std::cout << "[GatewayTCP:" << config_.plcId << "] Write Success. (Echo Match)\n";
    LOG_INFO("[GatewayTCP] Write Success. (Echo Match)");
    return true;
}

// -------------------------------------------------------------------
// 私有 TCP/IP 辅助方法实现
// -------------------------------------------------------------------

/**
 * @brief 实际的 socket 打开和连接操作
 */
bool GatewayTCPConnector::openSocket() {
    const auto& netConfig = config_.gatewayConfig;
    std::cout << "[TCP I/O] Connecting to " << netConfig.gatewayIp << ":" << netConfig.gatewayPort << "..." << std::endl;
    LOG_INFO("[TCP I/O] Connecting to " + netConfig.gatewayIp + ":" + std::to_string(netConfig.gatewayPort));

    // 1. 创建 socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "[TCP I/O] ERROR: Failed to create socket (" << strerror(errno) << ").\n";
        LOG_ERROR("[TCP I/O] Failed to create socket (" + std::string(strerror(errno)) + ")");
        return false;
    }
    
    // 2. 设置连接地址结构
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(netConfig.gatewayPort);

    // 3. 转换 IP 地址
    if (inet_pton(AF_INET, netConfig.gatewayIp.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "[TCP I/O] ERROR: Invalid address or address not supported (" << netConfig.gatewayIp << ").\n";
        LOG_ERROR("[TCP I/O] Invalid address or address not supported (" + netConfig.gatewayIp + ")");
        closeSocket();
        return false;
    }

    // 4. 设置连接超时 (例如 3 秒)
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    // 5. 连接服务器
    if (::connect(socket_fd_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "[TCP I/O] ERROR: Connection failed to " << netConfig.gatewayIp << ":" << netConfig.gatewayPort 
                  << " (" << strerror(errno) << ").\n";
        LOG_ERROR("[TCP I/O] Connection failed to " + netConfig.gatewayIp + ":" + std::to_string(netConfig.gatewayPort) +
                  " (" + std::string(strerror(errno)) + ")");
        closeSocket();
        return false;
    }

    std::cout << "[TCP I/O] Socket connected successfully (FD: " << socket_fd_ << ")" << std::endl;
    LOG_INFO("[TCP I/O] Socket connected successfully (FD: " + std::to_string(socket_fd_) + ")");
    return true; 
}

/**
 * @brief 实际的 socket 关闭操作
 */
void GatewayTCPConnector::closeSocket() {
    if (socket_fd_ != -1) {
        std::cout << "[TCP I/O] Closing socket (FD: " << socket_fd_ << ")." << std::endl;
        LOG_INFO("[TCP I/O] Closing socket (FD: " + std::to_string(socket_fd_) + ")");
        close(socket_fd_); 
        socket_fd_ = -1;
    }
}

/**
 * @brief 实际发送数据到 socket
 */
size_t GatewayTCPConnector::sendToSocket(const std::vector<char>& data) {
    if (socket_fd_ == -1 || data.empty()) return 0;
    
    ssize_t bytesWritten = send(socket_fd_, data.data(), data.size(), 0); 

    if (bytesWritten < 0) {
        std::cerr << "[TCP I/O] ERROR: Send failed (" << strerror(errno) << ").\n";
        LOG_ERROR("[TCP I/O] Send failed (" + std::string(strerror(errno)) + ")");
        return 0;
    }
    
    return static_cast<size_t>(bytesWritten); 
}

/**
 * @brief 实际从 socket 读取数据 (使用设置的 SO_RCVTIMEO)
 */
std::vector<char> GatewayTCPConnector::readFromSocket(size_t expectedMinBytes, int timeout_ms) {
    if (socket_fd_ == -1) return {};
    
    std::vector<char> receivedData;
    receivedData.reserve(expectedMinBytes + 16); // 预留空间
    
    char buffer[256];
    ssize_t bytesRead;
    
    // 循环读取直到满足最小字节数或超时
    auto startTime = std::chrono::steady_clock::now();

    while (receivedData.size() < expectedMinBytes) {
        // 使用 recv() 读取，它会受到 SO_RCVTIMEO 的影响
        bytesRead = recv(socket_fd_, buffer, sizeof(buffer), 0);
        
        if (bytesRead > 0) {
            receivedData.insert(receivedData.end(), buffer, buffer + bytesRead);
        } else if (bytesRead == 0) {
            // 连接关闭
            std::cout << "[TCP I/O] Connection closed by peer.\n";
            LOG_INFO("[TCP I/O] Connection closed by peer.");
            return {};
        } else if (bytesRead < 0) {
            // 发生错误 (包括超时)
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 超时 (如果设置了 SO_RCVTIMEO)
                // 检查是否已经超时 (额外检查)
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count() >= timeout_ms) {
                    std::cout << "[TCP I/O] Read Timeout.\n";
                    LOG_INFO("[TCP I/O] Read Timeout.");
                    break;
                }
                // 如果是 EAGAIN/EWOULDBLOCK 但未超时，可以继续等待，但由于设置了 SO_RCVTIMEO，这里通常就是超时了
            } else {
                std::cerr << "[TCP I/O] ERROR: Read failed (" << strerror(errno) << ").\n";
                LOG_ERROR("[TCP I/O] Read failed (" + std::string(strerror(errno)) + ")");
                return {};
            }
        }
    }
    
    return receivedData;
}

/**
 * @brief 将寄存器地址字符串 (例如 "0x0504") 转换为 Modbus 地址字节 (例如 {0x05, 0x04})。
 */
std::vector<char> GatewayTCPConnector::addressToBytes(const std::string& registerAddress) const {
    // 提取地址的后四个字符 (例如 "0504")
    if (registerAddress.length() < 6 || registerAddress.substr(0, 2) != "0x") {
        throw std::invalid_argument("Invalid register address format. Expected 0xXXXX.");
    }
    // "0x0504" -> "05 04" -> Bytes
    std::string cleanedAddress = registerAddress.substr(2, 2) + " " + registerAddress.substr(4, 2);
    
    return HexStringToBytes(cleanedAddress); 
}

/**
 * @brief 校验 Modbus TCP/IP 响应的基础结构和 MBAP 头。
 */
bool GatewayTCPConnector::validateResponse(const std::vector<char>& response, uint16_t transactionId, uint8_t expectedFuncCode, size_t expectedMinLength) const {
    if (response.empty()) {
        std::cout << "  <- RX Received: <Timeout/Empty>\n";
        LOG_ERROR("[GatewayTCP:" + config_.plcId + "] ERROR: Received timeout or empty response.");
        return false;
    }

    std::cout << "  <- RX Received: " << BytesToHexString(response) << "\n";

    if (response.size() < expectedMinLength) {
        std::cerr << "[GatewayTCP:" << config_.plcId << "] ERROR: Response too short (" << response.size() << " bytes).\n";
        LOG_ERROR("[GatewayTCP:" + config_.plcId + "] ERROR: Response too short (" + std::to_string(response.size()) + " bytes).");
        return false;
    }
    
    // 1. 校验 Transaction ID (MBAP 字节 0, 1)
    uint16_t rxTid = (static_cast<uint8_t>(response[0]) << 8) | static_cast<uint8_t>(response[1]);
    if (rxTid != transactionId) {
        std::cerr << "[GatewayTCP:" << config_.plcId << "] ERROR: Transaction ID mismatch. Expected " << transactionId << ", Got " << rxTid << ".\n";
        LOG_ERROR("[GatewayTCP:" + config_.plcId + "] ERROR: Transaction ID mismatch. Expected " + std::to_string(transactionId) + ", Got " + std::to_string(rxTid) + ".");
        return false;
    }
    
    // 2. 校验 Protocol ID (MBAP 字节 2, 3，必须为 0x0000)
    if (static_cast<uint8_t>(response[2]) != 0x00 || static_cast<uint8_t>(response[3]) != 0x00) {
        std::cerr << "[GatewayTCP:" << config_.plcId << "] ERROR: Protocol ID mismatch.\n";
        LOG_ERROR("[GatewayTCP:" + config_.plcId + "] ERROR: Protocol ID mismatch.");
        return false;
    }

    // 3. 校验 Unit ID (MBAP 字节 6)
    // 假设 config_.slaveId 包含了 PLC 的 Unit ID/Slave ID
    uint8_t rxUnitId = static_cast<uint8_t>(response[6]);
    if (rxUnitId != config_.slaveId) { 
        std::cerr << "[GatewayTCP:" << config_.plcId << "] ERROR: Unit ID mismatch. Expected " << (int)config_.slaveId << ", Got " << (int)rxUnitId << ".\n";
        LOG_ERROR("[GatewayTCP:" + config_.plcId + "] ERROR: Unit ID mismatch. Expected " + std::to_string((int)config_.slaveId) + ", Got " + std::to_string((int)rxUnitId) + ".");
        return false;
    }

    // 4. 校验功能码 (PDU 字节 0 / 响应字节 7)
    uint8_t funcCode = static_cast<uint8_t>(response[7]);
    
    if (funcCode == (expectedFuncCode | 0x80)) {
        // 异常响应 (功能码最高位为1)
        uint8_t exceptionCode = static_cast<uint8_t>(response[8]);
        std::cerr << "[GatewayTCP:" << config_.plcId << "] ERROR: Modbus Exception Code " << (int)exceptionCode << " (Func: " << (int)funcCode << ").\n";
        LOG_ERROR("[GatewayTCP:" + config_.plcId + "] ERROR: Modbus Exception Code " + std::to_string((int)exceptionCode) + " (Func: " + std::to_string((int)funcCode) + ").");
        return false;
    }
    
    if (funcCode != expectedFuncCode) {
        std::cerr << "[GatewayTCP:" << config_.plcId << "] ERROR: Function Code mismatch. Expected " << (int)expectedFuncCode << ", Got " << (int)funcCode << ".\n";
        LOG_ERROR("[GatewayTCP:" + config_.plcId + "] ERROR: Function Code mismatch. Expected " + std::to_string((int)expectedFuncCode) + ", Got " + std::to_string((int)funcCode) + ".");
        return false;
    }

    return true;
}

/**
 * @brief 内部连接检查：发送特定 Modbus TCP 报文以确认网关在线。
 */
bool GatewayTCPConnector::performHealthCheck(uint8_t unitId) {
    const uint8_t FUNC_READ_COILS = 0x01;
    const size_t MIN_RESPONSE_LENGTH = 9; // MBAP(7) + FC(1) + ByteCount(1)

    // PDU: 功能码01, 地址0x0000, 数量1
    std::vector<char> healthPDU = { 0x01, 0x00, 0x00, 0x00, 0x01 }; 
    uint16_t tid = next_transaction_id_.fetch_add(1);

    std::vector<char> healthCheckFrame = buildTCPFrame(healthPDU, tid, unitId);

    std::cout << "[GatewayTCP:" << config_.plcId << "] Sending Health Check to determine Gateway Online Status:\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(healthCheckFrame) << " (" << healthCheckFrame.size() << " bytes)\n";

    LOG_INFO("[GatewayTCP:" + config_.plcId + "] Sending Health Check to determine Gateway Online Status.");
    LOG_INFO("[GatewayTCP] TX Sent: " + BytesToHexString(healthCheckFrame));
    // 写入 Socket
    if (sendToSocket(healthCheckFrame) == 0) {
        std::cout << "[GatewayTCP:" << config_.plcId << "] Connection failed (Send error).\n";
        LOG_ERROR("[GatewayTCP:" + config_.plcId + "] Connection failed (Send error).");
        return false;
    }
    
    // 接收响应 (期望 9 字节)
    std::vector<char> response = readFromSocket(MIN_RESPONSE_LENGTH, 2000); 
    
    // 校验逻辑
    if (!validateResponse(response, tid, FUNC_READ_COILS, MIN_RESPONSE_LENGTH)) {
        std::cout << "[GatewayTCP:" << config_.plcId << "] Connection failed (Gateway Offline/Invalid Check Response).\n";
        LOG_ERROR("[GatewayTCP:" + config_.plcId + "] Connection failed (Gateway Offline/Invalid Check Response).");
        return false;
    }
    
    std::cout << "[GatewayTCP:" << config_.plcId << "] Health Check Success (Gateway Online)\n";
    LOG_INFO("[GatewayTCP:" + config_.plcId + "] Health Check Success (Gateway Online)");
    return true;
}