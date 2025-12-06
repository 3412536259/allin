// simulated_sensor.h
#ifndef SIMULATED_SENSOR_H
#define SIMULATED_SENSOR_H

#include "isensor.h"
#include "config_info.h"
#include <random>
#include <chrono>

class SimulatedSensor : public ISensor {
public:
    explicit SimulatedSensor(const SensorConfig& cfg) : cfg_(cfg) {
        simulateData();
    }

    bool init() override { return true; }
    void closeSerial() override {}
    bool readData() override {
        simulateData();
        return true;
    }

    std::string getId() const override { return cfg_.id; }
    float getTemperatureC() const override { return temperatureC_; }
    float getHumidityPct() const override { return humidityPct_; }
    float getValue() const override { return temperatureC_; } // 主值设为温度
    SensorStatus getStatus() const override { return SensorStatus::NORMAL; }

private:
    void simulateData() {
        static thread_local std::mt19937 rng(
            static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())
        );
        std::uniform_real_distribution<float> t(20.0f, 30.0f);
        std::uniform_real_distribution<float> h(30.0f, 70.0f);
        temperatureC_ = t(rng);
        humidityPct_ = h(rng);
    }

    SensorConfig cfg_;
    float temperatureC_ = 0.0f;
    float humidityPct_ = 0.0f;
};

#endif // SIMULATED_SENSOR_H