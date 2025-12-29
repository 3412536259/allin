#include "car_control_manager.h"
#include "config_parser.h"
#include <iostream>
#include <unistd.h>
#include <chrono>
#include"logger.h"
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
                LOG_ERROR("CarControlManager: failed to open port for " + cc.id);
                // still keep entry but mark not initialized
                ctx->initialized = false;
            } else {
                ctx->initialized = true;
            }
            devices_.emplace(cc.id, ctx);
            std::cout << "CarControlManager: loaded config for " << cc.id << "\n";
            LOG_INFO("CarControlManager: loaded config for " + cc.id);
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

    // capture a snapshot of the cancel sequence for this operation; if it changes while we're running
    // we should abort as superseded by a newer command. Note: we intentionally capture this before
    // acquiring the device mutex so that callers can increment cancelSeq without blocking.
    uint64_t mySeq = ctx->cancelSeq.load();

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
        // 检查是否被新的命令覆盖
        if (ctx->cancelSeq.load() != mySeq) {
        // 被覆盖，提前退出，但视为正常提前结束（返回与正常完成一致）
        res.success = true;
        res.motor1 = motor1;
        res.motor2 = motor2;
        res.statusByte = anyReply ? static_cast<uint16_t>(lastSt.statusByte) : static_cast<uint16_t>(-1);
        // persist into ctx for external queries
        ctx->lastStatusByte = res.statusByte;
        ctx->lastAnyReply = anyReply;
        LOG_INFO("CarControlManager: operate superseded for " + id + " (treated as normal completion)");
        return res;
        }
        // 发送控制命令
        ctx->driver.sendControl(m1, m2);
        
        // 在间隔时间内轮询读取状态
        int slept = 0;
        while (slept < interval_ms) {
            // 中断检查（尽量在短轮询间隔内快速响应）
            if (ctx->cancelSeq.load() != mySeq) {
                res.success = true;
                res.motor1 = motor1;
                res.motor2 = motor2;
                res.statusByte = anyReply ? static_cast<uint16_t>(lastSt.statusByte) : static_cast<uint16_t>(-1);
                ctx->lastStatusByte = res.statusByte;
                ctx->lastAnyReply = anyReply;
                LOG_INFO("CarControlManager: operate superseded during wait for " + id + " (treated as normal completion)");
                return res;
            }

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
                // update ctx last status
                ctx->lastStatusByte = static_cast<uint16_t>(st.statusByte);
                ctx->lastAnyReply = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(pollStepMs));
            slept += pollStepMs;
        }
        elapsed += interval_ms;
    }

    // 返回结果，包括响应时间
    res.success = true;
    res.motor1 = motor1;  // 回传发送的命令值，而非解析的值
    res.motor2 = motor2;
    res.statusByte = anyReply ? static_cast<uint16_t>(lastSt.statusByte) : static_cast<uint16_t>(-1);
    // persist into ctx
    ctx->lastStatusByte = res.statusByte;
    ctx->lastAnyReply = anyReply;
    // 如果之前没有设置responseTimeUs，则在这里设置超时值
    if (res.responseTimeUs == 0 && anyReply) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        res.responseTimeUs = duration.count();
    }
    return res;
}

void CarControlManager::interrupt(const std::string& id)
{
    auto it = devices_.find(id);
    if (it == devices_.end()) return;
    auto ctx = it->second;
    // increment cancel sequence to signal running operate() to abort
    uint64_t newVal = ctx->cancelSeq.fetch_add(1) + 1;
    LOG_INFO("CarControlManager: interrupt requested for " + id + ", new cancelSeq=" + std::to_string(newVal));
}

int CarControlManager::getLastStatus(const std::string& id)
{
    auto it = devices_.find(id);
    if (it == devices_.end()) return -1;
    auto ctx = it->second;
    std::lock_guard<std::mutex> lk(ctx->mtx);
    if (!ctx->lastAnyReply) return -1;
    return static_cast<int>(ctx->lastStatusByte);
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