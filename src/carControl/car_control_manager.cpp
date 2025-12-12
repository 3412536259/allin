#include "car_control_manager.h"
#include "config_parser.h"
#include <iostream>
#include <unistd.h>
#include <chrono>
#include <thread>

CarControlManager::CarControlManager()
{
    initFromConfig();
}

CarControlManager::~CarControlManager()
{
    shutdown();
}

bool CarControlManager::initFromConfig()
{
    try {
        const auto& cfg = ConfigParser::getInstance().getConfig();
        for (const auto& cc : cfg.carControls) {
            auto ctx = std::make_shared<DeviceCtx>();
            ctx->cfg = cc;
            if (!ctx->driver.init(cc.serial.port, cc.serial.baudRate > 0 ? cc.serial.baudRate : 9600)) {
                std::cerr << "CarControlManager: failed to open port for " << cc.id << "\n";
                // still keep entry but mark not initialized
                ctx->initialized = false;
            } else {
                ctx->initialized = true;
            }
            devices_.emplace(cc.id, ctx);
            std::cout << "CarControlManager: loaded config for " << cc.id << "\n";
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CarControlManager::initWithConfig(const CarControlConfig& cc)
{
    // initialize single device ctx (used if needed)
    auto it = devices_.find(cc.id);
    if (it != devices_.end()) {
        auto ctx = it->second;
        std::lock_guard<std::mutex> lk(ctx->mtx);
        ctx->cfg = cc;
        ctx->initialized = ctx->driver.init(cc.serial.port, cc.serial.baudRate > 0 ? cc.serial.baudRate : 9600);
        return ctx->initialized;
    }
    auto ctx = std::make_shared<DeviceCtx>();
    ctx->cfg = cc;
    ctx->initialized = ctx->driver.init(cc.serial.port, cc.serial.baudRate > 0 ? cc.serial.baudRate : 9600);
    devices_.emplace(cc.id, ctx);
    return ctx->initialized;
}

CarControlResult CarControlManager::operate(const std::string& id, int motor1, int motor2)
{
    CarControlResult res;
    auto it = devices_.find(id);
    if (it == devices_.end()) { res.success = false; res.message = "no such carcontrol id"; return res; }
    auto ctx = it->second;
    std::lock_guard<std::mutex> lk(ctx->mtx);
    if (!ctx->initialized) { res.success = false; res.message = "device not initialized"; return res; }

    // clamp values
    int16_t m1 = static_cast<int16_t>(std::max(-1500, std::min(1500, motor1)));
    int16_t m2 = static_cast<int16_t>(std::max(-1500, std::min(1500, motor2)));

    int total_ms = ctx->cfg.sendWindowMs;
    int interval_ms = ctx->cfg.sendIntervalMs;
    if (total_ms <= 0) total_ms = 600;
    if (interval_ms <= 0) interval_ms = 80;

    int elapsed = 0;
    MotorStatus lastSt{};
    bool anyReply = false;
    auto startTime = std::chrono::high_resolution_clock::now();

    // 持续发送命令帧并在期间监听响应
    const int pollStepMs = 20;
    while (elapsed <= total_ms) {
        // 发送控制命令
        ctx->driver.sendControl(m1, m2);
        
        // 在间隔时间内轮询读取状态
        int slept = 0;
        while (slept < interval_ms) {
            MotorStatus st;
            if (ctx->driver.readStatus(st)) {
                if (!anyReply) {
                    // 记录第一次收到响应的时间
                    auto endTime = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                    res.responseTimeUs = duration.count(); // 添加响应时间（微秒）
                }
                anyReply = true;
                lastSt = st;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(pollStepMs));
            slept += pollStepMs;
        }
        elapsed += interval_ms;
    }

    // 返回结果，包括响应时间
    res.success = true;
    //res.message.clear();
    res.motor1 = motor1;  // 回传发送的命令值，而非解析的值
    res.motor2 = motor2;
    res.statusByte = anyReply ? static_cast<uint16_t>(lastSt.statusByte) : 0;
    //ztl
    if (anyReply) {
        res.message = "Car control command executed successfully with response";
    } else {
        res.message = "Car control command executed failed (no response received)";
        res.statusByte = -1;
    }//ztl
    // 如果之前没有设置responseTimeUs，则在这里设置超时值
    if (res.responseTimeUs == 0 && anyReply) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        res.responseTimeUs = duration.count();
    }
    return res;
}

void CarControlManager::shutdown()
{
    for (auto &p : devices_) {
        if (!p.second) continue;
        try {
            std::lock_guard<std::mutex> lk(p.second->mtx);
            p.second->driver.closeSerial();
            p.second->initialized = false;
        } catch (...) {}
    }
}
