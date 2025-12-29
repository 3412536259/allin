#include "car_control.h"
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <cstring>
#include <iostream>
#include "logger.h"
#include <errno.h>
#include <sstream>
#include <iomanip> 

static inline uint16_t make_u16(uint8_t hi, uint8_t lo){ return (static_cast<uint16_t>(hi)<<8) | lo; }
static inline int16_t to_signed16(uint16_t v){ return *reinterpret_cast<int16_t*>(&v); }

static std::string bytesToHex(const uint8_t* bytes, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) ss << " ";
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}
CarControlDriver::~CarControlDriver(){ closeSerial(); }

bool CarControlDriver::init(const std::string& port, int baud)
{
    port_ = port;
    serial_fd_ = open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd_ < 0) {
        std::cerr << "CarControl: open serial failed: " << strerror(errno) << std::endl;
        LOG_ERROR("CarControl: open serial failed: " + std::string(strerror(errno)));
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_fd_, &tty) != 0) {
        std::cerr << "CarControl: tcgetattr failed: " << strerror(errno) << std::endl;
        LOG_ERROR("CarControl: tcgetattr failed: " + std::string(strerror(errno)));
        close(serial_fd_);
        serial_fd_ = -1;
        return false;
    }

    // baud
    speed_t speed = B9600;
    switch (baud) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 115200: speed = B115200; break;
        default: speed = B9600; break;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8N1
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_cflag |= CLOCAL | CREAD;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    // VTIME and VMIN: choose non-blocking read with small timeout
    tty.c_cc[VTIME] = 1; // tenths of seconds
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
        std::cerr << "CarControl: tcsetattr failed: " << strerror(errno) << std::endl;
        close(serial_fd_);
        serial_fd_ = -1;
        return false;
    }

    fcntl(serial_fd_, F_SETFL, 0);
    return true;
}

void CarControlDriver::closeSerial()
{
    if (serial_fd_ >= 0) {
        close(serial_fd_);
        serial_fd_ = -1;
    }
}

// build send frame according to spec and write
bool CarControlDriver::sendControl(int16_t motor1, int16_t motor2)
{
    if (serial_fd_ < 0) return false;
    uint8_t frame[8];
    frame[0] = 0x55;
    frame[1] = 0x5A;
    frame[2] = 0x05;
    uint16_t v1 = *reinterpret_cast<uint16_t*>(&motor1);
    uint16_t v2 = *reinterpret_cast<uint16_t*>(&motor2);
    frame[3] = (v1 >> 8) & 0xFF;
    frame[4] = v1 & 0xFF;
    frame[5] = (v2 >> 8) & 0xFF;
    frame[6] = v2 & 0xFF;
    frame[7] = 0x00; // reserved


    std::string hexStr = bytesToHex(frame, sizeof(frame));
    std::cout << "CarControl: Sending frame - Motor1: " << motor1 << ", Motor2: " << motor2 
              << ", Frame: " << hexStr << std::endl;
    LOG_INFO("CarControl: Sending frame - Motor1: " + std::to_string(motor1) + 
             ", Motor2: " + std::to_string(motor2) + ", Frame: " + hexStr);

    ssize_t n = write(serial_fd_, frame, sizeof(frame));



    if (n != (ssize_t)sizeof(frame)) {
        std::cerr << "CarControl: write failed: " << strerror(errno) << std::endl;
        LOG_ERROR("CarControl: write failed: " + std::string(strerror(errno)));
        return false;
    }
    tcdrain(serial_fd_);
    return true;
}

bool CarControlDriver::readStatus(MotorStatus& out)
{
    if (serial_fd_ < 0) return false;
    uint8_t buf[8];
    ssize_t n = read(serial_fd_, buf, sizeof(buf));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
        std::cerr << "CarControl: read error: " << strerror(errno) << std::endl;
        LOG_ERROR("CarControl: read error: " + std::string(strerror(errno)));
        return false;
    }
    if (n < 2) return false; // 至少需要2个字节才有意义

    // 如果读到的帧全部为 0，通常表示无设备或空噪声，应视为无响应（返回 false）
    bool allZero = true;
    for (ssize_t i = 0; i < n; ++i) {
        if (buf[i] != 0) { allZero = false; break; }
    }
    if (allZero) {
        LOG_INFO("CarControl: all-zero frame received -> treat as no reply (device likely not connected)");
        return false;
    }

    std::string hexStr = bytesToHex(buf, n);
    std::cout << "CarControl: Received frame: " << hexStr << " (" << n << " bytes)" << std::endl;
    LOG_INFO("CarControl: Received frame: " + hexStr + " (" + std::to_string(n) + " bytes)");
    // 只解析前两个字节作为状态字节
    out.statusByte = (static_cast<uint16_t>(buf[0]) << 8) | static_cast<uint16_t>(buf[1]);
    
    // 其余字节可以忽略或设置默认值
    out.motor1 = 0;
    out.motor2 = 0;
    
    return true;
}