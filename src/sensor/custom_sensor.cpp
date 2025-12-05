#include "custom_sensor.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

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
    status_ = SensorStatus::NORMAL;
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
    // 例如：检查帧头 0xAA，长度，校验和，提取温度湿度
    if (frame.size() < 6 || frame[0] != 0xAA) return false;

    // 假设 frame[1]=temp_high, frame[2]=temp_low → temp = (high<<8 | low)/10.0
    uint16_t raw_temp = (frame[1] << 8) | frame[2];
    temperatureC_ = raw_temp / 10.0f;
    humidityPct_ = frame[3]; // 示例

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