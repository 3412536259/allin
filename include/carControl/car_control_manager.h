#pragma once
#include <string>
#include "car_control.h"
#include "device_info.h"
#include "icar_control_manager.h"
#include "config_info.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <unordered_map>

class CarControlManager : public ICarControlManager {
public:
    CarControlManager();
    ~CarControlManager();

    bool initFromConfig();
    // initialize from a specific parsed CarControlConfig (single device)
    bool initWithConfig(const CarControlConfig& cc);

    // operate: send control and read reply; returns CarControlResult
    // synchronous operate: will send frames repeatedly until reply or timeout
    CarControlResult operate(const std::string& id, int motor1, int motor2) override;
    void shutdown() override;

private:
    struct DeviceCtx {
        CarControlConfig cfg;
        CarControlDriver driver;
        std::mutex mtx; // protect driver + per-device ops
        bool initialized = false;
    };

    std::unordered_map<std::string, std::shared_ptr<DeviceCtx>> devices_;
};
