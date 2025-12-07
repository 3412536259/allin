#ifndef CUSTOM_PROTOCOL_SENSOR_H
#define CUSTOM_PROTOCOL_SENSOR_H

#include "isensor.h"
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

class CustomProtocolSensor : public ISensor {
public:
    explicit CustomProtocolSensor(const SensorConfig& cfg);
    ~CustomProtocolSensor() override;

    bool init() override;
    void closeSerial() override;
    bool readData() override;

    std::string getId() const override { return cfg_.id; }
    std::string getType() const override { return "custom"; } // 新增
    float getTemperatureC() const override { return 0.0f; }   // 不适用
    float getHumidityPct() const override { return 0.0f; }    // 不适用
    float getValue() const override { return value_; }         // 返回自定义值
    SensorStatus getStatus() const override { return status_; }

private:
    SensorConfig cfg_;
    int serial_fd_ = -1;
    float value_ = 0.0f; // 自定义协议核心值（替换原温湿度）
    SensorStatus status_ = SensorStatus::OFFLINE;

    bool openSerial();
    bool sendRequest();
    bool receiveAndParse(std::vector<uint8_t>& out);
    bool parseCustomFrame(const std::vector<uint8_t>& frame);
};

#endif // CUSTOM_PROTOCOL_SENSOR_H