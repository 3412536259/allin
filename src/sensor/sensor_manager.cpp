#include "sensor_manager.h"
#include "modbus_sensor.h"    // 新增：ModbusSensor 声明
#include "gpio_sensor.h"      // 新增：GPIOSensor 声明
#include "custom_sensor.h"    // 新增：CustomProtocolSensor 声明
#include "config_info.h"
#include "sensor_types.h"       // 新增：getCurrentTimeStr 依赖
#include <iostream>           // 新增：std::cerr/std::cout 依赖
#include <algorithm>          // 新增：tolower 依赖（工厂函数）
#include <cctype>    

// 传感器工厂实现
std::unique_ptr<ISensor> createSensor(const SensorConfig& config) {
    std::string type = config.type;
    for (auto& c : type) c = static_cast<char>(std::tolower(c));

    if (type == "modbus" || type.empty()) {
        return std::make_unique<ModbusSensor>(config);
    }
    if (type == "gpio") {
        return std::make_unique<GPIOSensor>(config);
    }
    if (type == "custom" || type == "proprietary") {
        return std::make_unique<CustomProtocolSensor>(config);
    }

    // fallback
    return std::make_unique<ModbusSensor>(config);
}

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
            d.lastUpdateTime = getCurrentTimeStr();

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

SensorManager::~SensorManager() {
    stop();
}

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
        d.lastUpdateTime = getCurrentTimeStr();
        cache_[id] = d;
        return d;
    }
}

std::optional<SensorData> SensorManager::getSensorDataCached(const std::string& id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = cache_.find(id);
    if (it != cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<SensorData> SensorManager::getAllSensorData() {
    std::vector<SensorData> result;
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& pair : cache_) {
        result.push_back(pair.second);
    }
    return result;
}

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
    d.lastUpdateTime = getCurrentTimeStr();
    cache_[id] = d;
    return true;
}

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

void SensorManager::refreshLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(refreshIntervalSeconds_));
        if (!running_.load()) break;
        refreshAllSensors();
    }
}