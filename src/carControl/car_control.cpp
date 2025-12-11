#include "car_control.h"
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <cstring>
#include <iostream>
#include <errno.h>

static inline uint16_t make_u16(uint8_t hi, uint8_t lo){ return (static_cast<uint16_t>(hi)<<8) | lo; }
static inline int16_t to_signed16(uint16_t v){ return *reinterpret_cast<int16_t*>(&v); }

CarControlDriver::~CarControlDriver(){ closeSerial(); }

bool CarControlDriver::init(const std::string& port, int baud)
{
    port_ = port;
    serial_fd_ = open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd_ < 0) {
        std::cerr << "CarControl: open serial failed: " << strerror(errno) << std::endl;
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_fd_, &tty) != 0) {
        std::cerr << "CarControl: tcgetattr failed: " << strerror(errno) << std::endl;
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

    ssize_t n = write(serial_fd_, frame, sizeof(frame));
    if (n != (ssize_t)sizeof(frame)) {
        std::cerr << "CarControl: write failed: " << strerror(errno) << std::endl;
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
        return false;
    }
    if (n < 2) return false; // 至少需要2个字节才有意义

    // 只解析前两个字节作为状态字节
    out.statusByte = (static_cast<uint16_t>(buf[0]) << 8) | static_cast<uint16_t>(buf[1]);
    
    // 其余字节可以忽略或设置默认值
    out.motor1 = 0;
    out.motor2 = 0;
    
    return true;
}