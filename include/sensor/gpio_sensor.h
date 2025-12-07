#ifndef GPIO_SENSOR_H
#define GPIO_SENSOR_H

#include "isensor.h"
#include <fstream>
#include <string>
#include <cerrno>
#include <cstring>

class GPIOSensor : public ISensor {
public:
    explicit GPIOSensor(const SensorConfig& cfg) : cfg_(cfg) {}

    bool init() override;
    void closeSerial() override {}
    bool readData() override;

    std::string getId() const override { return cfg_.id; }
    float getTemperatureC() const override { return 0.0f; } // 不适用
    float getHumidityPct() const override { return 0.0f; }  // 不适用
    float getValue() const override { return value_; }       // 返回 GPIO 电平或计数
    SensorStatus getStatus() const override { return status_; }

private:
    SensorConfig cfg_;
    int gpioPin_ = -1;
    float value_ = 0.0f; // 可表示 0/1，或脉冲计数等
    SensorStatus status_ = SensorStatus::OFFLINE;

    bool exportGpio();
    int readGpioValue();
};

#endif // GPIO_SENSOR_H