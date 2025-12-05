#include "gpio_sensor.h"
#include <fstream>
#include <unistd.h>
#include <cstdlib>

bool GPIOSensor::exportGpio() {
    // 从 cfg_.serial.port 提取 GPIO 编号，例如 "gpio18" -> 18
    std::string port = cfg_.serial.port;
    if (port.substr(0, 4) != "gpio") return false;
    try {
        gpioPin_ = std::stoi(port.substr(4));
    } catch (...) {
        return false;
    }

    // 导出 GPIO（需 root 或权限）
    std::ofstream exportFile("/sys/class/gpio/export");
    if (!exportFile.is_open()) {
        status_ = SensorStatus::OFFLINE;
        return false;
    }
    exportFile << gpioPin_;
    exportFile.close();

    // 设置为输入
    std::string directionPath = "/sys/class/gpio/gpio" + std::to_string(gpioPin_) + "/direction";
    std::ofstream dirFile(directionPath);
    if (dirFile.is_open()) {
        dirFile << "in";
        dirFile.close();
    }

    status_ = SensorStatus::NORMAL;
    return true;
}

int GPIOSensor::readGpioValue() {
    std::string valuePath = "/sys/class/gpio/gpio" + std::to_string(gpioPin_) + "/value";
    std::ifstream valueFile(valuePath);
    if (!valueFile.is_open()) return -1;

    int val;
    valueFile >> val;
    return val;
}

bool GPIOSensor::init() {
    if (!exportGpio()) {
        std::cerr << "[GPIOSensor:" << cfg_.id << "] Failed to export GPIO\n";
        status_ = SensorStatus::OFFLINE;
        value_ = 0.0f;
        return false;
    }
    return true;
}

bool GPIOSensor::readData() {
    int val = readGpioValue();
    if (val < 0) {
        status_ = SensorStatus::ABNORMAL;
        return false;
    }
    value_ = static_cast<float>(val); // 或累积计数等逻辑
    status_ = SensorStatus::NORMAL;
    return true;
}