#include "mock_sensor.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <termios.h>
#include <random>
#include <cmath>

// ---------------- helpers ----------------
speed_t MockSensor::baudToSpeed(int baud) {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B9600;
    }
}

uint16_t MockSensor::crc16_modbus(const uint8_t* buf, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

bool MockSensor::writeExact(const uint8_t* data, size_t len) {
    if (serial_fd_ < 0) return false;
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(serial_fd_, data + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

bool MockSensor::readExact(uint8_t* buf, size_t len) {
    if (serial_fd_ < 0) return false;
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::read(serial_fd_, buf + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) {
            // VTIME timeout
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return total == len;
}

// ---------------- constructor / destructor ----------------
MockSensor::MockSensor(const SensorConfig& cfg) : cfg_(cfg) {
    // defaults for modbus fields; if you add those fields in JSON, set them here
    modbusAddr_ = 1;
    regStart_ = 0;
    regCount_ = 2;
    simulated_ = true;
    status_ = SensorStatus::OFFLINE;
}

MockSensor::~MockSensor() {
    closeSerial();
}

// ---------------- init / close ----------------
bool MockSensor::init() {
    // Use serial port info from cfg_.serial
    const SerialConfig& sc = cfg_.serial;

    serial_fd_ = open(sc.port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd_ < 0) {
        std::cerr << "[MockSensor:" << cfg_.id << "] open " << sc.port << " failed: " << strerror(errno)
                  << ". Using simulated mode.\n";
        simulated_ = true;
        status_ = SensorStatus::OFFLINE;
        serial_fd_ = -1;
        simulateData();
        return false;
    }

    struct termios tty{};
    if (tcgetattr(serial_fd_, &tty) != 0) {
        std::cerr << "[MockSensor:" << cfg_.id << "] tcgetattr failed: " << strerror(errno) << "\n";
        ::close(serial_fd_);
        serial_fd_ = -1;
        simulated_ = true;
        status_ = SensorStatus::OFFLINE;
        simulateData();
        return false;
    }

    speed_t sp = baudToSpeed(sc.baudRate);
    cfsetospeed(&tty, sp);
    cfsetispeed(&tty, sp);

    tty.c_cflag &= ~CSIZE;
    if (sc.baudRate == 7) tty.c_cflag |= CS7; // unlikely
    else tty.c_cflag |= CS8;

    // parity string can be "even" / "odd" / "none" or single char
    char p = 'N';
    if (!sc.parity.empty()) {
        std::string ps = sc.parity;
        for (auto &c : ps) c = static_cast<char>(tolower(c));
        if (ps == "even" || ps == "e") p = 'E';
        else if (ps == "odd" || ps == "o") p = 'O';
        else p = 'N';
    }
    if (p == 'E') {
        tty.c_cflag |= PARENB;
        tty.c_cflag &= ~PARODD;
    } else if (p == 'O') {
        tty.c_cflag |= PARENB;
        tty.c_cflag |= PARODD;
    } else {
        tty.c_cflag &= ~PARENB;
    }

    if (sc.stopBits == 2) tty.c_cflag |= CSTOPB;
    else tty.c_cflag &= ~CSTOPB;

    tty.c_cflag &= ~CRTSCTS;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~(OPOST | ONLCR);

    tty.c_cc[VTIME] = 10; // 1.0s
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
        std::cerr << "[MockSensor:" << cfg_.id << "] tcsetattr failed: " << strerror(errno) << "\n";
        ::close(serial_fd_);
        serial_fd_ = -1;
        simulated_ = true;
        status_ = SensorStatus::OFFLINE;
        simulateData();
        return false;
    }

    fcntl(serial_fd_, F_SETFL, 0); // blocking
    simulated_ = false;
    status_ = SensorStatus::NORMAL;
    return true;
}

void MockSensor::closeSerial() {
    if (serial_fd_ >= 0) {
        ::close(serial_fd_);
        serial_fd_ = -1;
    }
    simulated_ = true;
    status_ = SensorStatus::OFFLINE;
}

// ---------------- simulate / parse ----------------
void MockSensor::simulateData() {
    static thread_local std::mt19937 rng((unsigned)time(nullptr) ^ (uintptr_t)this);
    std::uniform_real_distribution<float> t(20.0f, 30.0f);
    std::uniform_real_distribution<float> h(30.0f, 70.0f);
    temperatureC_ = t(rng);
    humidityPct_ = h(rng);
    status_ = SensorStatus::NORMAL;
}

bool MockSensor::parseModbusResponse(const uint8_t* resp, size_t respLen) {
    // expected: addr(1) func(1) byteCount(1) data(2*regCount) crc_lo crc_hi
    if (respLen < 5) return false;
    if (resp[0] != static_cast<uint8_t>(modbusAddr_)) return false;
    if (resp[1] != 0x03) return false;
    uint8_t byteCount = resp[2];
    size_t expected = 3 + byteCount + 2;
    if (respLen != expected) return false;

    uint16_t crc_recv = static_cast<uint16_t>(resp[3 + byteCount]) |
                        (static_cast<uint16_t>(resp[3 + byteCount + 1]) << 8);
    uint16_t crc_calc = crc16_modbus(resp, 3 + byteCount);
    if (crc_recv != crc_calc) return false;

    if (byteCount < 4) return false;
    uint16_t raw_temp = (static_cast<uint16_t>(resp[3]) << 8) | resp[4];
    uint16_t raw_humi = (static_cast<uint16_t>(resp[5]) << 8) | resp[6];
    temperatureC_ = static_cast<float>(raw_temp) / 10.0f;
    humidityPct_ = static_cast<float>(raw_humi) / 10.0f;
    return true;
}

// ---------------- readData (Modbus request) ----------------
bool MockSensor::readData() {
    if (simulated_ || serial_fd_ < 0) {
        simulateData();
        return true;
    }

    uint8_t req[8];
    req[0] = static_cast<uint8_t>(modbusAddr_ & 0xFF);
    req[1] = 0x03;
    req[2] = static_cast<uint8_t>((regStart_ >> 8) & 0xFF);
    req[3] = static_cast<uint8_t>(regStart_ & 0xFF);
    req[4] = static_cast<uint8_t>((regCount_ >> 8) & 0xFF);
    req[5] = static_cast<uint8_t>(regCount_ & 0xFF);
    uint16_t crc = crc16_modbus(req, 6);
    req[6] = crc & 0xFF;
    req[7] = (crc >> 8) & 0xFF;

    if (!writeExact(req, sizeof(req))) {
        std::cerr << "[MockSensor:" << cfg_.id << "] write failed\n";
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    size_t expectedBytes = 3 + 2 * regCount_ + 2;
    std::vector<uint8_t> resp(expectedBytes);
    if (!readExact(resp.data(), expectedBytes)) {
        std::cerr << "[MockSensor:" << cfg_.id << "] read timeout/incomplete\n";
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    if (!parseModbusResponse(resp.data(), expectedBytes)) {
        std::cerr << "[MockSensor:" << cfg_.id << "] parseModbusResponse failed\n";
        status_ = SensorStatus::ABNORMAL;
        return false;
    }

    status_ = SensorStatus::NORMAL;
    return true;
}

// ---------------- getters ----------------
std::string MockSensor::getId() const { return cfg_.id; }
float MockSensor::getTemperatureC() const { return temperatureC_; }
float MockSensor::getHumidityPct() const { return humidityPct_; }
float MockSensor::getValue() const { return 123.45f; }
SensorStatus MockSensor::getStatus() const { return status_; }
int MockSensor::queryDataInt() {
    if (readData()) {
        return static_cast<int>(std::lround(temperatureC_ * 10.0f));
    }
    return 0;
}
