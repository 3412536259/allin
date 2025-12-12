#include "modbus_sensor.h"
#include <iostream>  // 新增：std::cerr 依赖
#include <ostream>   // 新增：std::endl 依赖
#include <cstdlib>   // 新增：rand() 依赖（simulateData 用）
#include <cstring>   // 新增：memset 依赖
#include <cerrno>  

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
    status_ = SensorStatus::NORMAL;
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

    std::string device = cfg_.serial.port;
    if (device.empty()) {
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    int baud = cfg_.serial.baudRate;
    if (baud <= 0) baud = 9600;

    // 1. 打开串口
    serial_fd_ = open(device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd_ < 0) {
        std::cerr << "[ModbusSensor] 串口打开失败：" << strerror(errno) << std::endl;
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    // 2. 配置串口属性
    termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_fd_, &tty) != 0) {
        std::cerr << "[ModbusSensor] 获取串口属性失败：" << strerror(errno) << std::endl;
        close(serial_fd_);
        serial_fd_ = -1;
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    // 波特率
    cfsetospeed(&tty, baudToSpeed(baud));
    cfsetispeed(&tty, baudToSpeed(baud));

    // 数据位/校验位/停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            // 8位数据位
    tty.c_cflag &= ~PARENB;        // 无校验
    tty.c_cflag &= ~CSTOPB;        // 1位停止位
    tty.c_cflag &= ~CRTSCTS;       // 禁用硬件流控

    // 模式配置
    tty.c_lflag &= ~ICANON;        // 非规范模式
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ISIG;

    // 输入处理
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // 输出处理
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    // 超时配置
    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;

    // 应用配置
    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
        std::cerr << "[ModbusSensor] 配置串口属性失败：" << strerror(errno) << std::endl;
        close(serial_fd_);
        serial_fd_ = -1;
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    // 恢复阻塞模式
    fcntl(serial_fd_, F_SETFL, 0);

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

    // 1. 构造Modbus请求帧
    uint8_t req[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};

    // 2. 清空接收缓冲区
    tcflush(serial_fd_, TCIFLUSH);

    // 3. 发送数据
    ssize_t bytes_written = write(serial_fd_, req, sizeof(req));
    if (bytes_written < 0 || (size_t)bytes_written != sizeof(req)) {
        std::cerr << "[ModbusSensor] 发送数据失败：" << strerror(errno) << std::endl;
        status_ = SensorStatus::OFFLINE;
        return false;
    }

    // 4. 等待响应
    usleep(200000);

    // 5. 读取响应
    uint8_t resp[100] = {0};
    ssize_t bytes_read = read(serial_fd_, resp, sizeof(resp));
    if (bytes_read < 0) {
        std::cerr << "[ModbusSensor] 读取数据失败：" << strerror(errno) << std::endl;
        status_ = SensorStatus::ABNORMAL;
        return false;
    }
    if (bytes_read == 0) {
        std::cerr << "[ModbusSensor] 未收到响应" << std::endl;
        status_ = SensorStatus::ABNORMAL;
        return false;
    }
    // std::cout<<"[ModbusSensor] Received "<<bytes_read<<std::endl;

    // 6. 解析响应
    if (bytes_read >= 9) {
        uint16_t tempRaw = (resp[3] << 8) | resp[4];
        uint16_t humRaw = (resp[5] << 8) | resp[6];
        temperatureC_ = tempRaw / 10.0f;
        humidityPct_ = humRaw / 10.0f;
        std::cout<<"[ModbusSensor] Temperature: "<<temperatureC_<<" C"<<std::endl;
        // std::cout<<"[ModbusSensor] Humidity: "<<humidityPct_<<" %"<<std::endl;
        status_ = SensorStatus::NORMAL;
        return true;
    } else {
        std::cerr << "[ModbusSensor] 响应长度不足：" << bytes_read << "字节" << std::endl;
        status_ = SensorStatus::ABNORMAL;
        return false;
    }
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

    status_ = SensorStatus::NORMAL;
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
    return temperatureC_;
}

SensorStatus ModbusSensor::getStatus() const {
    return status_;
}