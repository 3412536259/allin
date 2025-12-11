#pragma once

#include <string>
#include <vector>

struct MotorStatus {
    int16_t motor1 = 0; // -1500..1500
    int16_t motor2 = 0;
    uint16_t statusByte = 0; // may contain one or two status bytes
};

class CarControlDriver {
public:
    CarControlDriver() = default;
    ~CarControlDriver();

    bool init(const std::string& port, int baud = 9600);
    void closeSerial();

    // send control frame (call periodically or driver will schedule)
    bool sendControl(int16_t motor1, int16_t motor2);

    // read a single status frame from device (blocking read with timeout)
    bool readStatus(MotorStatus& out);

private:
    int serial_fd_ = -1;
    std::string port_; 
};
