#include "sensor_manager.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

// --- 构造函数 ---
SensorManager::SensorManager() {
    
        // 从 ConfigParser 单例加载
        const DeviceConfigRoot& root = ConfigParser::getInstance().getConfig();
        if (root.sensors.empty()) {
            std::cerr << "[SensorManager] WARNING: No sensors in global config.\n";
        } else {
            // 使用工厂创建传感器
            for (const auto& sconf : root.sensors) {
                auto sensor = createSensor(sconf);
                if (!sensor) {
                    std::cerr << "[SensorManager] Failed to create sensor: " << sconf.id << "\n";
                    continue;
                }

                // 初始化并读取初始值
                sensor->init();
                SensorData d;
                d.id = sconf.id;
                d.temperature = sensor->getTemperatureC();
                d.humidity = sensor->getHumidityPct();
                d.status = sensor->getStatus();

                {
                    std::lock_guard<std::mutex> lk(mu_);
                    sensors_.emplace(sconf.id, std::move(sensor));
                    cache_[sconf.id] = d;
                }
                std::cout << "[SensorManager] Loaded sensor " << sconf.id << "\n";
            }
        }
    
    // 启动后台刷新线程
    running_ = true;
    refreshThread_ = std::thread(&SensorManager::refreshLoop, this);
}

// --- 析构函数 ---
SensorManager::~SensorManager() {
    stop();
}

// --- stop ---
void SensorManager::stop() {
    if (running_.exchange(false)) {
        if (refreshThread_.joinable()) {
            refreshThread_.join();
        }
    }

    std::lock_guard<std::mutex> lk(mu_);
    for (auto& p : sensors_) {
        p.second->closeSerial();
    }
    sensors_.clear();
    cache_.clear();
}

// --- 实时读取 ---
std::optional<SensorData> SensorManager::getSensorDataRealTime(const std::string& id) {
    std::unique_ptr<ISensor>* sensorPtr = nullptr;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = sensors_.find(id);
        if (it == sensors_.end()) return std::nullopt;
        // 注意：我们不能返回 raw pointer 出临界区，所以直接在锁内操作
        if (!it->second->readData()) {
            return std::nullopt;
        }
        SensorData d;
        d.id = id;
        d.temperature = it->second->getTemperatureC();
        d.humidity = it->second->getHumidityPct();
        d.status = it->second->getStatus();
        cache_[id] = d;
        return d;
    }
}

// --- 缓存读取 ---
std::optional<SensorData> SensorManager::getSensorDataCached(const std::string& id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = cache_.find(id);
    if (it != cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// --- 刷新单个 ---
bool SensorManager::refreshSensor(const std::string& id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = sensors_.find(id);
    if (it == sensors_.end()) return false;

    if (!it->second->readData()) return false;

    SensorData d;
    d.id = id;
    d.temperature = it->second->getTemperatureC();
    d.humidity = it->second->getHumidityPct();
    d.status = it->second->getStatus();
    cache_[id] = d;
    return true;
}

// --- 刷新全部 ---
void SensorManager::refreshAllSensors() {
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lk(mu_);
        ids.reserve(sensors_.size());
        for (const auto& p : sensors_) {
            ids.push_back(p.first);
        }
    }

    for (const auto& id : ids) {
        refreshSensor(id);
    }
}

// --- 后台刷新循环 ---
void SensorManager::refreshLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(refreshIntervalSeconds_));
        if (!running_.load()) break;
        refreshAllSensors();
    }
}
