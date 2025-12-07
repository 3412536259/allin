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
    float getTemperatureC() const override { return temperatureC_; }
    float getHumidityPct() const override { return humidityPct_; }
    float getValue() const override { return temperatureC_; }
    SensorStatus getStatus() const override { return status_; }

private:
    SensorConfig cfg_;
    int serial_fd_ = -1;
    float temperatureC_ = 0.0f;
    float humidityPct_ = 0.0f;
    SensorStatus status_ = SensorStatus::OFFLINE;

    bool openSerial();
    bool sendRequest();
    bool receiveAndParse(std::vector<uint8_t>& out);
    bool parseCustomFrame(const std::vector<uint8_t>& frame);
};

#endif // CUSTOM_PROTOCOL_SENSOR_H