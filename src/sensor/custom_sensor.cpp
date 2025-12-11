#include "custom_sensor.h"
#include <iostream>  
#include <vector>    
#include <termios.h> 
#include <unistd.h>  
#include <fcntl.h>   
#include <cstring>   
#include <cerrno> 

CustomProtocolSensor::CustomProtocolSensor(const SensorConfig& cfg) : cfg_(cfg) {}

CustomProtocolSensor::~CustomProtocolSensor() {
    closeSerial();
}

bool CustomProtocolSensor::openSerial() {
    const auto& sc = cfg_.serial;
    serial_fd_ = open(sc.port.c_str(), O_RDWR | O_NOCTTY);
    if (serial_fd_ < 0) return false;

    struct termios tty;
    tcgetattr(serial_fd_, &tty);
    // TODO: 根据 sc 配置 baudRate, parity 等（参考 ModbusSensor）
    cfmakeraw(&tty);
    cfsetospeed(&tty, B9600); // 示例
    cfsetispeed(&tty, B9600);
    tty.c_cc[VTIME] = 5;
    tty.c_cc[VMIN] = 0;
    tcsetattr(serial_fd_, TCSANOW, &tty);

    return true;
}

bool CustomProtocolSensor::init() {
    if (!openSerial()) {
        std::cerr << "[CustomSensor:" << cfg_.id << "] Serial open failed\n";
        status_ = SensorStatus::OFFLINE;
        return false;
    }
    status_ = SensorStatus::OFFLINE;
    return true;
}

void CustomProtocolSensor::closeSerial() {
    if (serial_fd_ >= 0) {
        ::close(serial_fd_);
        serial_fd_ = -1;
    }
    status_ = SensorStatus::OFFLINE;
}

bool CustomProtocolSensor::sendRequest() {
    // 示例：发送 0xAA 0x01 0x00 0x00 0x55
    uint8_t req[] = {0xAA, 0x01, 0x00, 0x00, 0x55};
    return write(serial_fd_, req, sizeof(req)) == sizeof(req);
}

bool CustomProtocolSensor::receiveAndParse(std::vector<uint8_t>& out) {
    uint8_t buf[256];
    ssize_t n = read(serial_fd_, buf, sizeof(buf) - 1);
    if (n <= 0) return false;
    out.assign(buf, buf + n);
    return true;
}

bool CustomProtocolSensor::parseCustomFrame(const std::vector<uint8_t>& frame) {
    // TODO: 实现你的私有协议解析
    // 例如：检查帧头 0xAA，长度，校验和，提取自定义值
    if (frame.size() < 6 || frame[0] != 0xAA) return false;

    // 示例：提取自定义值（frame[1]和frame[2]拼接）
    uint16_t raw_val = (frame[1] << 8) | frame[2];
    value_ = raw_val / 10.0f; // 自定义值赋值

    return true;
}

bool CustomProtocolSensor::readData() {
    if (serial_fd_ < 0) {
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    if (!sendRequest()) {
        status_ = SensorStatus::ABNORMAL;
        return false;
    }

    std::vector<uint8_t> resp;
    if (!receiveAndParse(resp)) {
        status_ = SensorStatus::ABNORMAL;
        return false;
    }

    if (!parseCustomFrame(resp)) {
        status_ = SensorStatus::ABNORMAL;
        return false;
    }

    status_ = SensorStatus::NORMAL;
    return true;
}