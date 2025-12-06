#include "gateway_tcp_connector.h"
#include "serial_plc_connector.h"
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

// -------------------------------------------------------------------
// 辅助函数实现 (与 serial_plc_connector.cpp 中的辅助函数保持一致，但去除了 CRC 计算)
// -------------------------------------------------------------------

// /**
//  * @brief 将 Hex 字符串转换为字节向量
//  */
// std::vector<char> HexStringToBytes(const std::string& hexFrame) {
//     std::vector<char> bytes;
//     std::stringstream ss(hexFrame);
//     std::string byteString;
//     while (ss >> byteString) {
//         if (byteString.length() == 2) {
//             try {
//                 bytes.push_back(static_cast<char>(std::stoul(byteString, nullptr, 16)));
//             } catch (const std::exception& e) {
//                 std::cerr << "Hex conversion error for byte: " << byteString << std::endl;
//             }
//         }
//     }
//     return bytes;
// }

/**
 * @brief 将字节数组转换为 Hex 字符串用于打印日志
 */
inline std::string BytesToHexString(const std::vector<char>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : data) {
        ss << std::setw(2) << (static_cast<int>(byte) & 0xFF) << " ";
    }
    std::string result = ss.str();
    if (!result.empty()) {
        result.pop_back(); // 移除末尾空格
    }
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// -------------------------------------------------------------------
// GatewayTCPConnector 类实现
// -------------------------------------------------------------------

GatewayTCPConnector::GatewayTCPConnector(const PLCConfig& config)
    : PLCConnector(config), socket_fd_(-1), next_transaction_id_(1) {
    // 确保 Unit ID 存在，Modbus RTU Slave ID 0x01
    // 对于网关，通常使用 Slave ID 0x01
    if (config_.plcId.empty()) { 
        std::cerr << "[GatewayTCP] WARNING: PLC ID is empty, connection might fail.\n";
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
        return true;
    }

    // 1. 打开 Socket 并连接
    if (!openSocket()) {
        status_ = "PORT_ERROR";
        return false;
    }

    // 2. 准备健康检查报文 (RTU PDU: 01 00 00 00 01)
    // 功能码01 读取线圈，地址0x0000，数量1
    std::vector<char> healthPDU = { 0x01, 0x00, 0x00, 0x00, 0x01 };
    uint16_t tid = next_transaction_id_.fetch_add(1) % 65535;
    uint8_t unitId = 0x01; // 假设网关的 Slave ID 为 1
    
    std::vector<char> healthCheckFrame = buildTCPFrame(healthPDU, tid, unitId);

    std::cout << "[GatewayTCP:" << config_.plcId << "] Sending Health Check to determine Gateway Online Status:\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(healthCheckFrame) << " (" << healthCheckFrame.size() << " bytes)\n";

    // 3. 写入 Socket
    if (sendToSocket(healthCheckFrame) == 0) {
        closeSocket();
        status_ = "DISCONNECTED";
        return false;
    }
    
    // 4. 接收响应 (期望 MBAP Header (7) + PDU (Function Code (1) + Byte Count (1) + Data (1)) = 10 bytes)
    std::vector<char> response = readFromSocket(9, 2000); 
    
    if (response.empty()) {
        status_ = "DISCONNECTED";
        std::cout << "  <- RX Received: <Timeout/No Reply>\n";
        std::cout << "[GatewayTCP:" << config_.plcId << "] Connection failed (Gateway Offline).\n";
        closeSocket(); 
        return false;
    }

    // 5. 校验逻辑 (检查最小长度和功能码)
    // 响应帧的第 7 字节是功能码
    if (response.size() < 9 || response[7] != 0x01) { 
        std::cout << "  <- RX Received: " << BytesToHexString(response) << " (Invalid Reply - Gateway Status UNCERTAIN)\n";
        status_ = "UNCERTAIN";
        closeSocket(); 
        return false;
    }
    
    status_ = "CONNECTED";
    std::cout << "  <- RX Received: " << BytesToHexString(response) << " (Gateway Online)\n";
    std::cout << "[GatewayTCP:" << config_.plcId << "] Connection established (Gateway Online).\n";
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
        return "ERROR";
    }
    
    // 1. 构造 PDU (功能码 01: Read Coils)
    std::string cleanedAddress = address.substr(2, 2) + " " + address.substr(4, 2);
    std::vector<char> addrBytes = HexStringToBytes(cleanedAddress);

    // PDU structure: Func(1) Addr_Hi(1) Addr_Lo(1) Count_Hi(1) Count_Lo(1)
    std::vector<char> readPDU = { 
        0x01, // Function Code (Read Coils)
        addrBytes[0], addrBytes[1], // Register Address 0x0504
        0x00, 0x01 // Read 1 coil
    };

    uint16_t tid = next_transaction_id_.fetch_add(1) % 65535;
    uint8_t unitId = 0x01; // 假设 Slave ID 为 1
    std::vector<char> readFrame = buildTCPFrame(readPDU, tid, unitId);

    std::cout << "[GatewayTCP:" << config_.plcId << "] Reading Device Status on Address " << address << ":\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(readFrame) << " (" << readFrame.size() << " bytes)\n";
    
    // 2. 写入 Socket
    if (sendToSocket(readFrame) == 0) return "ERROR";
    
    // 3. 读取响应 (期望 9 字节: MBAP(7) + FC(1) + ByteCount(1) + Data(n))
    std::vector<char> response = readFromSocket(9); 

    // 4. 真实解析和判断逻辑
    if (response.empty()) {
        std::cout << "  <- RX Received: <Timeout/Error>\n";
        return "ERROR";
    }

    // 检查功能码和长度
    // 响应帧的第 7 字节是功能码
    if (response.size() >= 9 && response[7] == 0x01) {
        // response[9] 是数据位。对于单个线圈，数据位是 0x01 (ON) 或 0x00 (OFF)
        std::string status = (response[9] & 0x01) ? "1" : "0";
        std::cout << "  <- RX Received: " << BytesToHexString(response) << " (Status: " << status << ")\n";
        return status;
    }

    std::cout << "  <- RX Received: " << BytesToHexString(response) << " (Invalid Response)\n";
    return "ERROR";
}

/**
 * @brief 写入寄存器的值（Modbus TCP）。
 */
bool GatewayTCPConnector::writeRegister(const std::string& address, const std::string& value) {
    std::lock_guard<std::mutex> lock(io_mutex_);

    if (status_ != "CONNECTED" || socket_fd_ == -1) {
        std::cout << "[GatewayTCP:" << config_.plcId << "] ERROR: Write failed, Gateway is DISCONNECTED." << std::endl;
        return false;
    }

    std::vector<char> dataValue;
    if (value == "1" || value == "ON") {
        dataValue = { (char)0xFF, (char)0x00 }; // ON: FF 00
    } else if (value == "0" || value == "OFF") {
        dataValue = { (char)0x00, (char)0x00 }; // OFF: 00 00
    } else {
        std::cerr << "[GatewayTCP:" << config_.plcId << "] Invalid write value: " << value << std::endl;
        return false;
    }

    // 1. 构造 PDU (功能码 05: Write Single Coil)
    std::string cleanedAddress = address.substr(2, 2) + " " + address.substr(4, 2);
    std::vector<char> addrBytes = HexStringToBytes(cleanedAddress);

    // PDU structure: Func(1) Addr_Hi(1) Addr_Lo(1) Data_Hi(1) Data_Lo(1)
    std::vector<char> writePDU = { 
        0x05, // Function Code (Write Single Coil)
        addrBytes[0], addrBytes[1], // Register Address 0x0504
        dataValue[0], dataValue[1] // Coil Value (FF00 or 0000)
    };
    
    uint16_t tid = next_transaction_id_.fetch_add(1) % 65535;
    uint8_t unitId = 0x01; // 假设 Slave ID 为 1
    std::vector<char> writeFrame = buildTCPFrame(writePDU, tid, unitId);

    std::cout << "[GatewayTCP:" << config_.plcId << "] Writing Device Status on Address " << address << " with value " << value << ":\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(writeFrame) << " (" << writeFrame.size() << " bytes)\n";
    
    // 2. 写入 Socket
    if (sendToSocket(writeFrame) == 0) return false;
    
    // 3. 读取响应 (期望 Echo Frame, 12 字节: MBAP(7) + PDU(5))
    std::vector<char> response = readFromSocket(writeFrame.size()); 

    // 4. 真实解析和判断逻辑 (Modbus TCP Echo Frame 校验)
    if (response.empty()) {
        std::cout << "  <- RX Received: <Timeout/Error>\n";
        return false;
    }
    
    // 校验 Echo Frame: 检查长度、功能码和数据是否与发送帧匹配
    // 检查 PDU 部分 (从第 7 字节开始)
    bool success = (response.size() == writeFrame.size() && 
                    // 校验 Function Code (第 7 字节)
                    response[7] == writeFrame[7] && 
                    // 校验 Address (第 8, 9 字节)
                    response[8] == writeFrame[8] && 
                    response[9] == writeFrame[9] &&
                    // 校验 Data (第 10, 11 字节)
                    response[10] == writeFrame[10] && 
                    response[11] == writeFrame[11]); 
                    
    // 真实项目还会校验 MBAP 头中的 Transaction ID

    std::cout << "  <- RX Received (Echo Frame): " << BytesToHexString(response) << (success ? " (Match)" : " (Mismatch)") << "\n";
    return success;
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

    // 1. 创建 socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "[TCP I/O] ERROR: Failed to create socket (" << strerror(errno) << ").\n";
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
        closeSocket();
        return false;
    }

    std::cout << "[TCP I/O] Socket connected successfully (FD: " << socket_fd_ << ")" << std::endl;
    return true; 
}

/**
 * @brief 实际的 socket 关闭操作
 */
void GatewayTCPConnector::closeSocket() {
    if (socket_fd_ != -1) {
        std::cout << "[TCP I/O] Closing socket (FD: " << socket_fd_ << ")." << std::endl;
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
            return {};
        } else if (bytesRead < 0) {
            // 发生错误 (包括超时)
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 超时 (如果设置了 SO_RCVTIMEO)
                // 检查是否已经超时 (额外检查)
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count() >= timeout_ms) {
                    std::cout << "[TCP I/O] Read Timeout.\n";
                    break;
                }
                // 如果是 EAGAIN/EWOULDBLOCK 但未超时，可以继续等待，但由于设置了 SO_RCVTIMEO，这里通常就是超时了
            } else {
                std::cerr << "[TCP I/O] ERROR: Read failed (" << strerror(errno) << ").\n";
                return {};
            }
        }
    }
    
    return receivedData;
}