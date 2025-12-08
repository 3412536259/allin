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

    // 1. 打开串口并配置 (I/O 逻辑)
    if (!openSerialPort()) {
        status_ = "PORT_ERROR";
        return false;
    }

    // 2. 执行 Modbus 级别的连接检查 (业务逻辑)
    if (!performHealthCheck()) {
        // HealthCheck 失败时，它内部已经打印了失败原因
        // 必须关闭串口，因为连接失败
        closeSerialPort(); 
        status_ = "DISCONNECTED";
        return false;
    }
    
    // 3. 连接成功
    status_ = "CONNECTED";
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
    
    const uint8_t FUNC_READ_COILS = 0x01;
    const size_t MIN_RESPONSE_LENGTH = 6; // SlaveID FuncCode ByteCount Data CRC

    // 1. 构造读取报文 (Modbus 报文数据层)
    std::vector<char> addrBytes;
    try {
        addrBytes = addressToBytes(address);
    } catch (const std::invalid_argument& e) {
        std::cerr << "[SerialPLC:" << config_.plcId << "] ERROR: " << e.what() << std::endl;
        return "ERROR";
    }

    // Modbus Data: Address_Hi, Address_Lo, Quantity_Hi, Quantity_Lo (Read 1 coil)
    std::vector<char> readData = { 
        addrBytes[0], addrBytes[1], 
        0x00, 0x01 
    };

    std::vector<char> readFrame = buildModbusFrame(FUNC_READ_COILS, readData);

    std::cout << "[SerialPLC:" << config_.plcId << "] Reading Device Status on Address " << address << ":\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(readFrame) << " (" << readFrame.size() << " bytes)\n";
    
    // 2. 帧交换
    std::vector<char> response = exchangeFrame(readFrame, MIN_RESPONSE_LENGTH, 2000);

    // 3. 校验和解析
    if (!validateResponse(response, FUNC_READ_COILS, MIN_RESPONSE_LENGTH)) {
        return "ERROR";
    }

    // 响应的第 4 个字节 (response[3]) 是数据位。
    // 对于单个线圈，数据位是 0x01 (ON) 或 0x00 (OFF)
    std::string status = (response[3] & 0x01) ? "1" : "0";
    std::cout << "[SerialPLC:" << config_.plcId << "] Read Success. (Status: " << status << ")\n";
    return status;
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
    
    const uint8_t FUNC_WRITE_SINGLE_COIL = 0x05;
    const size_t EXPECTED_ECHO_LENGTH = 8; // Echo frame: 8 bytes

    // 确定写入数据值 (FF00 或 0000)
    std::vector<char> dataValue;
    if (value == "1" || value == "ON") {
        dataValue = { (char)0xFF, (char)0x00 }; // ON: FF 00
    } else if (value == "0" || value == "OFF") {
        dataValue = { (char)0x00, (char)0x00 }; // OFF: 00 00
    } else {
        std::cerr << "[SerialPLC:" << config_.plcId << "] Invalid write value: " << value << std::endl;
        return false;
    }

    // 1. 构造写入报文 (Modbus 报文数据层)
    std::vector<char> addrBytes;
    try {
        addrBytes = addressToBytes(address);
    } catch (const std::invalid_argument& e) {
        std::cerr << "[SerialPLC:" << config_.plcId << "] ERROR: " << e.what() << std::endl;
        return false;
    }

    // Modbus Data: Address_Hi, Address_Lo, Value_Hi, Value_Lo
    std::vector<char> writeData = { 
        addrBytes[0], addrBytes[1], 
        dataValue[0], dataValue[1] 
    };
    
    std::vector<char> writeFrame = buildModbusFrame(FUNC_WRITE_SINGLE_COIL, writeData);

    std::cout << "[SerialPLC:" << config_.plcId << "] Writing Device Status on Address " << address << " with value " << value << ":\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(writeFrame) << " (" << writeFrame.size() << " bytes)\n";
    
    // 2. 帧交换
    std::vector<char> response = exchangeFrame(writeFrame, EXPECTED_ECHO_LENGTH, 2000);

    // 3. 校验 Echo Frame
    if (!validateResponse(response, FUNC_WRITE_SINGLE_COIL, EXPECTED_ECHO_LENGTH)) {
        return false;
    }

    // 4. 进一步校验 Echo Frame 是否完全匹配 (写操作特有)
    // 检查响应帧是否与请求帧完全一致 (包含 CRC，因为 validateResponse 只做了基础检查)
    bool isEchoMatch = (response.size() == writeFrame.size() && 
                        std::equal(response.begin(), response.end(), writeFrame.begin()));

    if (!isEchoMatch) {
        std::cerr << "[SerialPLC:" << config_.plcId << "] ERROR: Write failed. Echo frame mismatch.\n";
        return false;
    }
    
    std::cout << "[SerialPLC:" << config_.plcId << "] Write Success. (Echo Match)\n";
    return true;
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

/**
 * @brief 构建 Modbus RTU 报文帧，附加 CRC 校验码。
 */
std::vector<char> SerialPLCConnector::buildModbusFrame(uint8_t funcCode, const std::vector<char>& data) const {
    std::vector<char> frame;
    frame.push_back(static_cast<char>(config_.slaveId)); // Slave ID
    frame.push_back(static_cast<char>(funcCode));        // Function Code
    frame.insert(frame.end(), data.begin(), data.end()); // Data

    // 计算CRC
    unsigned short crc = calculate_crc16(frame);
    frame.push_back((char)(crc & 0xFF));        // CRC Low Byte
    frame.push_back((char)((crc >> 8) & 0xFF)); // CRC High Byte

    return frame;
}

/**
 * @brief 发送并接收 Modbus RTU 报文帧。
 */
std::vector<char> SerialPLCConnector::exchangeFrame(const std::vector<char>& txFrame, size_t expectedMinBytes, int timeout_ms) {
    writeToSerial(txFrame);

    std::vector<char> response = readFromSerial(expectedMinBytes, timeout_ms);

    if(response.empty()) {
        std::cout << "  <- RX Received: <Timeout/Error>\n";
    } else {
        std::cout << "  <- RX Received: " << BytesToHexString(response) << "\n";
        // 此处应校验CRC
    }

    return response;
}

bool SerialPLCConnector::validateResponse(const std::vector<char>& response, uint8_t expectedFuncCode, size_t expectedMinLength) const{
    if(response.empty()){
        std::cerr << "[SerialPLC:" << config_.plcId << "] ERROR: Response timeout/empty.\n";
        return false;
    }
    if (response.size() < expectedMinLength) {
        std::cerr << "[SerialPLC:" << config_.plcId << "] ERROR: Response too short (" << response.size() << " bytes).\n";
        return false;
    }
    
    // 1. 校验 Slave ID
    if (static_cast<uint8_t>(response[0]) != config_.slaveId) {
        std::cerr << "[SerialPLC:" << config_.plcId << "] ERROR: Slave ID mismatch. Expected " << (int)config_.slaveId << ", Got " << (int)response[0] << ".\n";
        return false;
    }

    // 2. 校验功能码（检查是否为异常响应）
    uint8_t funcCode = static_cast<uint8_t>(response[1]);
    if (funcCode == (expectedFuncCode | 0x80)) {
        // 异常响应 (功能码最高位为1)
        uint8_t exceptionCode = static_cast<uint8_t>(response[2]);
        std::cerr << "[SerialPLC:" << config_.plcId << "] ERROR: Modbus Exception Code " << (int)exceptionCode << " (Func: " << (int)funcCode << ").\n";
        return false;
    }
    
    // 3. 校验功能码（检查是否为期望功能码）
    if (funcCode != expectedFuncCode) {
        std::cerr << "[SerialPLC:" << config_.plcId << "] ERROR: Function Code mismatch. Expected " << (int)expectedFuncCode << ", Got " << (int)funcCode << ".\n";
        return false;
    }

    // 4. 真实项目中，此处应执行 **CRC 校验**。
    
    return true;
}

/**
 * @brief 内部连接检查：发送特定 Modbus 报文以确认 PLC 在线。
 */
bool SerialPLCConnector::performHealthCheck() {
    const uint8_t FUNC_READ_COILS = 0x01;
    const size_t MIN_RESPONSE_LENGTH = 6; // 01 01 01 XX CRC_L CRC_H

    // 报文数据: 地址 0x0000, 读取 1 个线圈
    std::vector<char> checkData = { 0x00, 0x00, 0x00, 0x01 };
    std::vector<char> healthCheckFrame = buildModbusFrame(FUNC_READ_COILS, checkData);
    
    std::cout << "[SerialPLC:" << config_.plcId << "] Sending Health Check to determine PLC Online Status:\n";
    std::cout << "  -> TX Sent: " << BytesToHexString(healthCheckFrame) << " (" << healthCheckFrame.size() << " bytes)\n";
    
    std::vector<char> response = exchangeFrame(healthCheckFrame, MIN_RESPONSE_LENGTH, 2000); // 2000ms Timeout

    if (!validateResponse(response, FUNC_READ_COILS, MIN_RESPONSE_LENGTH)) {
        std::cout << "[SerialPLC:" << config_.plcId << "] Connection failed (PLC Offline/Invalid Check Response).\n";
        return false;
    }
    
    return true;
}

/**
 * @brief 将寄存器地址字符串 (例如 "0x0504") 转换为 Modbus 地址字节 (例如 {0x05, 0x04})。
 */
std::vector<char> SerialPLCConnector::addressToBytes(const std::string& registerAddress) const {
    // 提取地址的后四个字符 (例如 "0504")
    if (registerAddress.length() < 6 || registerAddress.substr(0, 2) != "0x") {
        throw std::invalid_argument("Invalid register address format. Expected 0xXXXX.");
    }

    // "0x0504" -> "05 04" -> Bytes
    std::string cleanedAddress = registerAddress.substr(2, 2) + " " + registerAddress.substr(4, 2);
    return HexStringToBytes(cleanedAddress);
}