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

    // request an interrupt for the currently running operation on device `id`.
    // This increments a per-device cancel sequence so a running `operate` will detect
    // the change and abort early (returning a superseded result).
    void interrupt(const std::string& id);

    void shutdown() override;

private:
    struct DeviceCtx {
        CarControlConfig cfg;
        CarControlDriver driver;
        std::mutex mtx; // protect driver + per-device ops
        bool initialized = false;
        // increment this atomic to signal cancellation/superseding of currently running operate()
        std::atomic<uint64_t> cancelSeq{0};
        // last read status from device: if no reply seen, lastAnyReply==false
        uint16_t lastStatusByte = 0;
        bool lastAnyReply = false;
    };

    std::unordered_map<std::string, std::shared_ptr<DeviceCtx>> devices_;

    // query last observed status byte for device id; returns -1 if no reply seen
    int getLastStatus(const std::string& id);
};