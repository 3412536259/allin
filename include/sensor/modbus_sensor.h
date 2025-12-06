// modbus_sensor.h
#ifndef MODBUS_SENSOR_H
#define MODBUS_SENSOR_H

#include "isensor.h"
#include "config_info.h"
#include <cstdint>
#include <string>
#include <vector>
#include <termios.h>
#include <unistd.h>

class ModbusSensor : public ISensor {
public:
    explicit ModbusSensor(const SensorConfig& cfg);
    ~ModbusSensor() override;

    bool init() override;
    void closeSerial() override;
    bool readData() override;

    std::string getId() const override;
    float getTemperatureC() const override;
    float getHumidityPct() const override;
    float getValue() const override;
    SensorStatus getStatus() const override;

private:
    SensorConfig cfg_;
    int serial_fd_ = -1;
    bool simulated_ = true; // fallback when no serial
    float temperatureC_ = 0.0f;
    float humidityPct_ = 0.0f;
    SensorStatus status_ = SensorStatus::OFFLINE;

    int modbusAddr_ = 1;
    int regStart_ = 0;
    int regCount_ = 2;

    static speed_t baudToSpeed(int baud);
    static uint16_t crc16_modbus(const uint8_t* data, size_t len);
    bool writeExact(const uint8_t* data, size_t len);
    bool readExact(uint8_t* buf, size_t len);
    void simulateData();
    bool parseModbusResponse(const uint8_t* resp, size_t respLen);
};

#endif // MODBUS_SENSOR_H