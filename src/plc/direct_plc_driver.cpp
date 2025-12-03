#include "DirectPLCDriver.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

DirectPLCDriver::DirectPLCDriver(const PLCConfig& cfg)
    : config_(cfg) {}

DirectPLCDriver::~DirectPLCDriver() {
    stop();
}

bool DirectPLCDriver::start() {
    serialFd_ = open(config_.serial.port.c_str(), O_RDWR | O_NOCTTY);

    if (serialFd_ < 0) {
        std::cerr << "[DirectPLCDriver] open serial failed\n";
        state_ = PLCState::OFFLINE;
        return false;
    }

    // 这里你需要自己配置串口参数（简化）
    state_ = PLCState::ONLINE;
    std::cout << "[DirectPLCDriver] Serial PLC started\n";
    return true;
}

void DirectPLCDriver::stop() {
    if (serialFd_ > 0) {
        close(serialFd_);
        serialFd_ = -1;
    }
    state_ = PLCState::OFFLINE;
}

bool DirectPLCDriver::readRegister(const std::string& addr, int& outValue) {
    std::lock_guard<std::mutex> lk(commMutex_);
    if (state_ == PLCState::OFFLINE) return false;

    // 这里写成伪代码
    outValue = 1;  // 假设读成功了
    return true;
}

bool DirectPLCDriver::writeRegister(const std::string& addr, int value) {
    std::lock_guard<std::mutex> lk(commMutex_);
    if (state_ == PLCState::OFFLINE) return false;

    // 简化伪代码
    std::cout << "[DirectPLCDriver] Write " << value 
              << " to " << addr << "\n";

    return true;
}
