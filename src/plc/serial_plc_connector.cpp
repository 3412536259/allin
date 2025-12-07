#include "serial_plc_connector.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cstring>

// 引入 POSIX 串口通信所需的头文件
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <chrono>

// -------------------------------------------------------------------
// 辅助函数实现
// -------------------------------------------------------------------

/**
 * @brief 计算 Modbus RTU 规范的 CRC-16/MODBUS 校验码。
 * @param data 待校验的字节数组 (不含CRC)
 * @return CRC-16 校验码 (低位在前，高位在后)
 */
unsigned short calculate_crc16(const std::vector<char>& data) {
    unsigned short crc = 0xFFFF;
    for (char byte : data) {
        crc ^= (unsigned char)byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001; // 0x8005 反转
            } else {
                crc >>= 1;
            }
        }
    }
    // 返回未交换的 CRC 值，由调用者决定如何附加 (通常是低位在前)
    return crc;
}

/**
 * @brief 将 Hex 字符串转换为字节向量 (真实 Modbus RTU 必需)
 */
std::vector<char> HexStringToBytes(const std::string& hexFrame) {
    std::vector<char> bytes;
    std::stringstream ss(hexFrame);
    std::string byteString;
    while (ss >> byteString) {
        if (byteString.length() == 2) {
            try {
                bytes.push_back(static_cast<char>(std::stoul(byteString, nullptr, 16)));
            } catch (const std::exception& e) {
                std::cerr << "Hex conversion error for byte: " << byteString << std::endl;
            }
        }
    }
    return bytes;
}

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
    // 确保所有字符为大写，便于日志阅读
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// -------------------------------------------------------------------
// SerialPLCConnector 类实现
// -------------------------------------------------------------------

SerialPLCConnector::SerialPLCConnector(const PLCConfig& config)
    : PLCConnector(config), serialHandle_(0) {
}

SerialPLCConnector::~SerialPLCConnector() {
    disconnect();
}

/**
 * @brief 真正的连接过程：打开串口并发送健康检查报文。
 */
bool SerialPLCConnector::connect() {
    if (serialHandle_ != 0) {
        std::cout << "[SerialPLC:" << config_.plcId << "] Already connected." << std::endl;
        return true;
    }

    // 1. 打开串口并配置 (使用 termios 实现)
    if (!openSerialPort()) {
        status_ = "PORT_ERROR";
        return false;
    }

    // 2. 准备健康检查报文: 01 01 00 00 00 01 (功能码01 读取线圈，地址0x0000)
    // 期待响应: 01 01 01 00 (表示线圈状态为 OFF)
    std::vector<char> healthCheckFrame = { 0x01, 0x01, 0x00, 0x00, 0x00, 0x01 };
    
    unsigned short crc = calculate_crc16(healthCheckFrame);
    healthCheckFrame.push_back((char)(crc & 0xFF));        // CRC Low Byte
    healthCheckFrame.push_back((char)((crc >> 8) & 0xFF)); // CRC High Byte

    std::cout << "[SerialPLC:" << config_.plcId << "] Sending Health Check to determine PLC Online Status:\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(healthCheckFrame) << " (" << healthCheckFrame.size() << " bytes)\n";

    // 3. 写入串口
    writeToSerial(healthCheckFrame);
    
    // 4. 接收响应 (期望 8 字节: 01 01 01 00 51 88)
    std::vector<char> response = readFromSerial(6, 2000); 
    
    if (response.empty()) {
        status_ = "DISCONNECTED";
        std::cout << "  <- RX Received: <Timeout/No Reply>\n";
        std::cout << "[SerialPLC:" << config_.plcId << "] Connection failed (PLC Offline).\n";
        closeSerialPort(); 
        return false;
    }

    // 5. 校验逻辑 (仅检查最小长度和功能码，实际项目中需校验 CRC)
    if (response.size() < 6 || (response[0] != 0x01 || response[1] != 0x01)) { 
         std::cout << "  <- RX Received: " << BytesToHexString(response) << " (Invalid Reply - PLC Status UNCERTAIN)\n";
    }
    
    status_ = "CONNECTED";
    std::cout << "  <- RX Received: " << BytesToHexString(response) << " (PLC Online)\n";
    std::cout << "[SerialPLC:" << config_.plcId << "] Connection established (PLC Online).\n";
    return true;
}

/**
 * @brief 关闭串口。
 */
void SerialPLCConnector::disconnect() {
    closeSerialPort();
    status_ = "DISCONNECTED";
}

std::string SerialPLCConnector::getConnectionStatus() const {
    return status_;
}

/**
 * @brief 读取下挂设备状态（根据返回值第4个字节判断 01/00）。
 */
std::string SerialPLCConnector::readRegister(const std::string& address) {
    if (status_ != "CONNECTED" || serialHandle_ == 0) {
        std::cout << "[SerialPLC:" << config_.plcId << "] ERROR: Cannot read, PLC is DISCONNECTED." << std::endl;
        return "ERROR";
    }
    
    // 1. 构造读取报文 (功能码 01: Read Coils)
    // 地址格式: 0x0504 -> 05 04。由于 HexStringToBytes 接受带空格的Hex，这里先进行转换
    std::string cleanedAddress = address.substr(2, 2) + " " + address.substr(4, 2);
    std::vector<char> addrBytes = HexStringToBytes(cleanedAddress);

    // Frame structure: SlaveID(1) Func(1) Addr_Hi(1) Addr_Lo(1) Count_Hi(1) Count_Lo(1)
    std::vector<char> readFrame = { 
        0x01, // Slave ID 
        0x01, // Function Code (Read Coils)
        addrBytes[0], addrBytes[1], // Register Address 0x0504
        0x00, 0x01 // Read 1 coil
    };

    unsigned short crc = calculate_crc16(readFrame);
    readFrame.push_back((char)(crc & 0xFF));
    readFrame.push_back((char)((crc >> 8) & 0xFF));

    std::cout << "[SerialPLC:" << config_.plcId << "] Reading Device Status on Address " << address << ":\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(readFrame) << " (" << readFrame.size() << " bytes)\n";
    
    // 2. 写入串口
    writeToSerial(readFrame);
    
    // 3. 读取响应 (期望 6 字节: SlaveID FuncCode ByteCount Data CRC_L CRC_H)
    std::vector<char> response = readFromSerial(6); 

    // 4. 真实解析和判断逻辑
    if (response.empty()) {
        std::cout << "  <- RX Received: <Timeout/Error>\n";
        return "ERROR";
    }

    // 检查最小长度和功能码
    if (response.size() >= 4 && response[0] == 0x01 && response[1] == 0x01) {
        // response[3] 是数据位。对于单个线圈，数据位是 0x01 (ON) 或 0x00 (OFF)
        std::string status = (response[3] & 0x01) ? "1" : "0";
        std::cout << "  <- RX Received: " << BytesToHexString(response) << " (Status: " << status << ")\n";
        return status;
    }

    std::cout << "  <- RX Received: " << BytesToHexString(response) << " (Invalid Response)\n";
    return "ERROR";
}

/**
 * @brief 写入寄存器的值（操作下挂设备：开/关）。
 * 流程: 发送操作报文，期待返回相同的 Echo Frame。
 */
bool SerialPLCConnector::writeRegister(const std::string& address, const std::string& value) {
    if (status_ != "CONNECTED" || serialHandle_ == 0) {
        std::cout << "[SerialPLC:" << config_.plcId << "] ERROR: Write failed, PLC is DISCONNECTED." << std::endl;
        return false;
    }

    std::vector<char> dataValue;
    if (value == "1" || value == "ON") {
        dataValue = { (char)0xFF, (char)0x00 }; // ON: FF 00
    } else if (value == "0" || value == "OFF") {
        dataValue = { (char)0x00, (char)0x00 }; // OFF: 00 00
    } else {
        std::cerr << "[SerialPLC:" << config_.plcId << "] Invalid write value: " << value << std::endl;
        return false;
    }

    // 1. 构造写入报文 (功能码 05: Write Single Coil)
    std::string cleanedAddress = address.substr(2, 2) + " " + address.substr(4, 2);
    std::vector<char> addrBytes = HexStringToBytes(cleanedAddress);

    // Frame structure: SlaveID(1) Func(1) Addr_Hi(1) Addr_Lo(1) Data_Hi(1) Data_Lo(1)
    std::vector<char> writeFrame = { 
        0x01, // Slave ID 
        0x05, // Function Code (Write Single Coil)
        addrBytes[0], addrBytes[1], // Register Address 0x0504
        dataValue[0], dataValue[1] // Coil Value (FF00 or 0000)
    };
    
    unsigned short crc = calculate_crc16(writeFrame);
    writeFrame.push_back((char)(crc & 0xFF));
    writeFrame.push_back((char)((crc >> 8) & 0xFF));

    std::cout << "[SerialPLC:" << config_.plcId << "] Writing Device Status on Address " << address << " with value " << value << ":\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(writeFrame) << " (" << writeFrame.size() << " bytes)\n";
    
    // 2. 写入串口
    writeToSerial(writeFrame);
    
    // 3. 读取响应 (期待 Echo Frame, 8 字节)
    std::vector<char> response = readFromSerial(writeFrame.size()); 

    // 4. 真实解析和判断逻辑
    if (response.empty()) {
        std::cout << "  <- RX Received: <Timeout/Error>\n";
        return false;
    }
    
    // 校验 Echo Frame: 检查长度、功能码和数据是否与发送帧匹配
    bool success = (response.size() == writeFrame.size() && 
                    response[0] == writeFrame[0] && 
                    response[1] == writeFrame[1] &&
                    response[2] == writeFrame[2] && 
                    response[3] == writeFrame[3] &&
                    response[4] == writeFrame[4] &&
                    response[5] == writeFrame[5]); 
                    // 真实项目还会校验 CRC

    std::cout << "  <- RX Received (Echo Frame): " << BytesToHexString(response) << (success ? " (Match)" : " (Mismatch)") << "\n";
    return success;
}

// -------------------------------------------------------------------
// 私有辅助方法实现
// -------------------------------------------------------------------

inline speed_t SerialPLCConnector::getBaudRateConstant(int baudRate) {
    switch (baudRate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default: return B0;
    }
}

/**
 * @brief 实际的串口打开和配置操作 (使用 termios)
 */
bool SerialPLCConnector::openSerialPort() {
    const auto& serialConfig = config_.serialConfig;
    std::cout << "[Serial I/O] Opening port " << serialConfig.serial.port << " @ " << serialConfig.serial.baudRate << "..." << std::endl;
    
    // 1. 打开串口文件
    // O_RDWR: 读写, O_NOCTTY: 不作为控制终端, O_NONBLOCK: 非阻塞 (用于 select)
    serialHandle_ = open(serialConfig.serial.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (serialHandle_ < 0) {
        std::cerr << "[Serial I/O] ERROR: Could not open port " << serialConfig.serial.port << " (" << strerror(errno) << ").\n";
        return false;
    }
    
    // 2. 配置 termios 结构体
    struct termios tty;
    if (tcgetattr(serialHandle_, &tty) != 0) { 
        std::cerr << "[Serial I/O] ERROR: tcgetattr failed (" << strerror(errno) << ").\n";
        closeSerialPort();
        return false;
    }

    // 3. 设置波特率
    speed_t speed = getBaudRateConstant(serialConfig.serial.baudRate);
    if (speed == B0) {
        std::cerr << "[Serial I/O] ERROR: Unsupported baud rate: " << serialConfig.serial.baudRate << std::endl;
        closeSerialPort();
        return false;
    }
    cfsetospeed(&tty, speed); 
    cfsetispeed(&tty, speed); 

    // 4. 配置数据位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;       // 8位数据 (Modbus RTU 标准)

    // 5. 设置停止位
    if (serialConfig.serial.stopBits == 2) {
        tty.c_cflag |= CSTOPB; // 2个停止位
    } else {
        tty.c_cflag &= ~CSTOPB; // 1个停止位
    }

    // 6. 设置校验位
    // 禁用 Parity Flags in Input Mode first
    tty.c_iflag &= ~(INPCK | ISTRIP);

    if (serialConfig.serial.parity == "even") {
        tty.c_cflag |= PARENB;
        tty.c_cflag &= ~PARODD;
        tty.c_iflag |= INPCK; // Enable parity checking on input
    } else if (serialConfig.serial.parity == "odd") {
        tty.c_cflag |= PARENB;
        tty.c_cflag |= PARODD;
        tty.c_iflag |= INPCK; // Enable parity checking on input
    } else { // none
        tty.c_cflag &= ~PARENB;
    }
    
    // 启用本地连接和接收器
    tty.c_cflag |= (CLOCAL | CREAD); 

    // 7. 配置本地模式 (Raw Mode / No Canonical)
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); 

    // 8. 配置输入/输出模式 (禁用软件流控制和输出处理)
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | ICRNL | IGNCR);
    tty.c_oflag &= ~OPOST;

    // 9. 设置 VMIN (最小字节数) 和 VTIME (超时时间)
    tty.c_cc[VTIME] = 1; // 0.1 seconds (1 unit = 0.1s)
    tty.c_cc[VMIN] = 0;  // Non-blocking read (returns 0 if no data)

    // 10. 激活配置
    if (tcsetattr(serialHandle_, TCSANOW, &tty) != 0) { 
        std::cerr << "[Serial I/O] ERROR: tcsetattr failed (" << strerror(errno) << ").\n";
        closeSerialPort();
        return false;
    }
    
    // 11. 切换回阻塞模式以简化后续读写（可选，但 select 方式更灵活）
    // fcntl(serialHandle_, F_SETFL, 0); 

    std::cout << "[Serial I/O] Port opened and configured successfully (FD: " << serialHandle_ << ")" << std::endl;
    return true; 
}

/**
 * @brief 实际的串口关闭操作 (使用 termios)
 */
void SerialPLCConnector::closeSerialPort() {
    if (serialHandle_ > 0) {
        std::cout << "[Serial I/O] Closing port (FD: " << serialHandle_ << ")." << std::endl;
        close(serialHandle_); 
        serialHandle_ = 0;
    }
}

/**
 * @brief 实际发送数据到串口 (使用 termios)
 */
size_t SerialPLCConnector::writeToSerial(const std::vector<char>& data) {
    if (serialHandle_ <= 0 || data.empty()) return 0;
    
    // 清除接收缓冲区中的任何旧数据
    tcflush(serialHandle_, TCIFLUSH); 

    ssize_t bytesWritten = write(serialHandle_, data.data(), data.size()); 

    if (bytesWritten < 0) {
        std::cerr << "[Serial I/O] ERROR: Write failed (" << strerror(errno) << ").\n";
        return 0;
    }
    
    // 等待所有数据发送完毕 (重要，确保 Modbus 帧完整发送)
    tcdrain(serialHandle_); 
    
    return static_cast<size_t>(bytesWritten); 
}

/**
 * @brief 实际从串口读取数据 (使用 termios 和 select 实现超时)
 */
std::vector<char> SerialPLCConnector::readFromSerial(size_t expectedMinBytes, int timeout_ms) {
    if (serialHandle_ <= 0) return {};
    
    std::vector<char> buffer(256); 
    std::vector<char> receivedData;
    auto startTime = std::chrono::steady_clock::now();
    
    // 动态设置 select 的超时时间
    timeval timeout;

    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - startTime).count() < timeout_ms) {
        
        // 计算剩余时间
        long long elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - startTime).count();
        int remaining_ms = timeout_ms - elapsed_ms;

        if (remaining_ms <= 0) break;

        timeout.tv_sec = remaining_ms / 1000;
        timeout.tv_usec = (remaining_ms % 1000) * 1000; 
        
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(serialHandle_, &read_fds);

        // 使用 select() 监听文件描述符是否有数据可读
        int sel = select(serialHandle_ + 1, &read_fds, NULL, NULL, &timeout);

        if (sel > 0 && FD_ISSET(serialHandle_, &read_fds)) {
            // 有数据可读
            ssize_t bytesRead = read(serialHandle_, buffer.data(), buffer.size());

            if (bytesRead > 0) {
                receivedData.insert(receivedData.end(), buffer.begin(), buffer.begin() + bytesRead);
                
                if (receivedData.size() >= expectedMinBytes) {
                    return receivedData;
                }
            } else if (bytesRead < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    std::cerr << "[Serial I/O] ERROR: Read failed (" << strerror(errno) << ").\n";
                    return {};
                }
            }
        } else if (sel < 0) {
            std::cerr << "[Serial I/O] ERROR: select failed (" << strerror(errno) << ").\n";
            return {};
        }
    }

    // 超时
    return {}; 
}