// modbus_sensor.cpp
#include "modbus_sensor.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib> // for rand()

// Helper: Convert baud rate to termios speed_t
speed_t ModbusSensor::baudToSpeed(int baud) {
    switch (baud) {
        case 1200:   return B1200;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B9600;
    }
}

// CRC16-MODBUS calculation
uint16_t ModbusSensor::crc16_modbus(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool ModbusSensor::writeExact(const uint8_t* data, size_t len) {
    if (serial_fd_ < 0) return false;
    ssize_t sent = 0;
    while (sent < static_cast<ssize_t>(len)) {
        ssize_t n = write(serial_fd_, data + sent, len - sent);
        if (n <= 0) return false;
        sent += n;
    }
    tcdrain(serial_fd_);
    return true;
}

bool ModbusSensor::readExact(uint8_t* buf, size_t len) {
    if (serial_fd_ < 0) return false;
    size_t received = 0;
    while (received < len) {
        ssize_t n = read(serial_fd_, buf + received, len - received);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

void ModbusSensor::simulateData() {
    static float temp = 22.0f;
    static float hum = 45.0f;
    temp += (rand() % 100 - 50) / 100.0f; // ±0.5
    hum  += (rand() % 100 - 50) / 100.0f;
    if (temp < 20.0f) temp = 20.0f;
    if (temp > 30.0f) temp = 30.0f;
    if (hum < 40.0f) hum = 40.0f;
    if (hum > 60.0f) hum = 60.0f;

    temperatureC_ = temp;
    humidityPct_ = hum;
    status_ = SensorStatus::NORMAL; // ✅ 使用 NORMAL 表示正常
}

ModbusSensor::ModbusSensor(const SensorConfig& cfg) : cfg_(cfg) {
    std::string device = cfg_.serial.port;
    simulated_ = device.empty();

    modbusAddr_ = (cfg_.modbusAddr > 0) ? cfg_.modbusAddr : 1;
    regStart_   = cfg_.regStart;
    regCount_   = (cfg_.regCount > 0) ? cfg_.regCount : 2;

    status_ = simulated_ ? SensorStatus::NORMAL : SensorStatus::OFFLINE;
}

ModbusSensor::~ModbusSensor() {
    closeSerial();
}

bool ModbusSensor::init() {
    if (simulated_) {
        status_ = SensorStatus::NORMAL;
        return true;
    }

    std::string device = cfg_.serial.port; // ✅ 正确
    if (device.empty()) {
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    int baud = cfg_.serial.baudRate; // ✅ 正确
    if (baud <= 0) baud = 9600;

    serial_fd_ = open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd_ < 0) {
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    struct termios tty;
    if (tcgetattr(serial_fd_, &tty) != 0) {
        close(serial_fd_);
        serial_fd_ = -1;
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    cfmakeraw(&tty);
    cfsetspeed(&tty, baudToSpeed(baud));
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
        close(serial_fd_);
        serial_fd_ = -1;
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    status_ = SensorStatus::NORMAL;
    return true;
}

void ModbusSensor::closeSerial() {
    if (serial_fd_ >= 0) {
        close(serial_fd_);
        serial_fd_ = -1;
    }
}

bool ModbusSensor::readData() {
    if (simulated_) {
        simulateData();
        return true;
    }

    if (serial_fd_ < 0 || status_ == SensorStatus::OFFLINE) {
        return false;
    }

    // Build Modbus RTU request: [addr][func][start_hi][start_lo][count_hi][count_lo][crc_lo][crc_hi]
    uint8_t req[8];
    req[0] = static_cast<uint8_t>(modbusAddr_);
    req[1] = 0x03; // Read Holding Registers
    req[2] = static_cast<uint8_t>((regStart_ >> 8) & 0xFF);
    req[3] = static_cast<uint8_t>(regStart_ & 0xFF);
    req[4] = static_cast<uint8_t>((regCount_ >> 8) & 0xFF);
    req[5] = static_cast<uint8_t>(regCount_ & 0xFF);
    uint16_t crc = crc16_modbus(req, 6);
    req[6] = static_cast<uint8_t>(crc & 0xFF);
    req[7] = static_cast<uint8_t>((crc >> 8) & 0xFF);

    if (!writeExact(req, 8)) {
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    size_t respLen = 3 + 2 * regCount_ + 2;
    uint8_t* resp = new uint8_t[respLen];
    bool ok = readExact(resp, respLen);
    if (ok) {
        ok = parseModbusResponse(resp, respLen);
    }
    delete[] resp;

    if (!ok) {
        status_ = SensorStatus::ABNORMAL; // ✅ 通信失败 → 异常
        return false;
    }

    return true;
}

bool ModbusSensor::parseModbusResponse(const uint8_t* resp, size_t respLen) {
    if (respLen < 5) return false;
    if (resp[0] != modbusAddr_) return false;
    if (resp[1] != 0x03) return false; // function code mismatch

    uint16_t crcRecv = (static_cast<uint16_t>(resp[respLen - 2]) << 8) | resp[respLen - 1];
    uint16_t crcCalc = crc16_modbus(resp, respLen - 2);
    if (crcRecv != crcCalc) return false;

    // Assume first register = temperature * 10, second = humidity * 10
    if (regCount_ >= 1) {
        uint16_t tempRaw = (static_cast<uint16_t>(resp[3]) << 8) | resp[4];
        temperatureC_ = tempRaw / 10.0f;
    }
    if (regCount_ >= 2) {
        uint16_t humRaw = (static_cast<uint16_t>(resp[5]) << 8) | resp[6];
        humidityPct_ = humRaw / 10.0f;
    }

    status_ = SensorStatus::NORMAL; // ✅ 解析成功
    return true;
}

std::string ModbusSensor::getId() const {
    return cfg_.id;
}

float ModbusSensor::getTemperatureC() const {
    return temperatureC_;
}

float ModbusSensor::getHumidityPct() const {
    return humidityPct_;
}

float ModbusSensor::getValue() const {
    return temperatureC_; // or customize
}

SensorStatus ModbusSensor::getStatus() const {
    return status_;
}