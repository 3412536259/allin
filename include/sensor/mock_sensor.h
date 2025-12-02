#ifndef MOCK_SENSOR_H
#define MOCK_SENSOR_H

#include "isensor.h"
#include "config_info.h"   // <- 你已经提供的类型定义（SensorConfig / SerialConfig）
#include <cstdint>
#include <string>
#include <vector>
#include <termios.h> // for speed_t
#include <unistd.h> // for read/write/close

class MockSensor : public ISensor {
public:
    explicit MockSensor(const SensorConfig& cfg);
    ~MockSensor();

    bool init() override;
    void closeSerial() override;

    bool readData() override;

    std::string getId() const override;
    float getTemperatureC() const override;
    float getHumidityPct() const override;
    float getValue() const override ;
    SensorStatus getStatus() const override;

    // 如果需要即时读取并返回 int（温度*10）可用
    int queryDataInt();

private:
    SensorConfig cfg_;
    int serial_fd_ = -1;
    bool simulated_ = true; // fallback when no serial
    float temperatureC_ = 0.0f;
    float humidityPct_ = 0.0f;
    SensorStatus status_ = SensorStatus::OFFLINE;

    // Modbus params (defaults; you can extend to read from JSON)
    int modbusAddr_ = 1;
    int regStart_ = 0;
    int regCount_ = 2;

    // helpers
    static speed_t baudToSpeed(int baud);
    static uint16_t crc16_modbus(const uint8_t* data, size_t len);
    bool writeExact(const uint8_t* data, size_t len);
    bool readExact(uint8_t* buf, size_t len);
    void simulateData();
    bool parseModbusResponse(const uint8_t* resp, size_t respLen);
};

#endif // MOCK_SENSOR_H
