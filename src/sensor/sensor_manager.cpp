#include "sensor_manager.h"
#include <iostream>
#include <chrono>
#include <thread>

SensorManager::SensorManager() : running_(false) {}

SensorManager::~SensorManager() {
    stop();
}

bool SensorManager::start(const std::string& configPath, int refreshIntervalSeconds) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!loadSensors(configPath)) {
        std::cerr << "[SensorManager] loadSensors failed\n";
        return false;
    }
    refreshIntervalSeconds_ = refreshIntervalSeconds;
    running_.store(true);
    refreshThread_ = std::thread(&SensorManager::refreshLoop, this);

    // initial sync refresh
    refreshAllSensors();
    return true;
}

void SensorManager::stop() {
    if (running_.load()) {
        running_.store(false);
        if (refreshThread_.joinable()) refreshThread_.join();
    }
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& p : sensors_) p.second->closeSerial();
    sensors_.clear();
    cache_.clear();
}

std::optional<SensorData> SensorManager::getSensorDataRealTime(const std::string& id) {
    MockSensor* s = nullptr;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = sensors_.find(id);
        if (it == sensors_.end()) return std::nullopt;
        s = it->second.get();
    }
    if (!s) return std::nullopt;

    bool ok = s->readData();
    SensorData d;
    d.id = id;
    d.temperature = s->getTemperatureC();
    d.humidity = s->getHumidityPct();
    d.status = s->getStatus();

    {
        std::lock_guard<std::mutex> lk(mu_);
        cache_[id] = d;
    }
    return d;
}

std::optional<SensorData> SensorManager::getSensorDataCached(const std::string& id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = cache_.find(id);
    if (it == cache_.end()) return std::nullopt;
    return it->second;
}

bool SensorManager::refreshSensor(const std::string& id) {
    MockSensor* s = nullptr;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = sensors_.find(id);
        if (it == sensors_.end()) return false;
        s = it->second.get();
    }
    if (!s) return false;
    bool ok = s->readData();
    SensorData d;
    d.id = id;
    d.temperature = s->getTemperatureC();
    d.humidity = s->getHumidityPct();
    d.status = s->getStatus();

    std::lock_guard<std::mutex> lk(mu_);
    cache_[id] = d;
    return ok;
}

void SensorManager::refreshAllSensors() {
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lk(mu_);
        ids.reserve(sensors_.size());
        for (auto& p : sensors_) ids.push_back(p.first);
    }

    for (auto& id : ids) refreshSensor(id);
}

void SensorManager::refreshLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(refreshIntervalSeconds_));
        if (!running_.load()) break;
        refreshAllSensors();
    }
}

bool SensorManager::loadSensors(const std::string& configPath) {
    ConfigParser parser;
    if (!parser.loadFromFile(configPath)) {
        std::cerr << "[SensorManager] ConfigParser load failed: " << configPath << "\n";
        return false;
    }

    const DeviceConfigRoot& root = parser.getConfig();
    if (root.sensors.empty()) {
        std::cerr << "[SensorManager] No sensors in config\n";
        return false;
    }

    for (const auto& sconf : root.sensors) {
        SensorConfig cfg = sconf; // use your existing SensorConfig type
        // create sensor instance
        auto s = std::make_unique<MockSensor>(cfg);
        s->init(); // will fallback to simulated if serial fails

        SensorData d;
        d.id = cfg.id;
        d.temperature = s->getTemperatureC();
        d.humidity = s->getHumidityPct();
        d.status = s->getStatus();

        std::lock_guard<std::mutex> lk(mu_);
        cache_[cfg.id] = d;
        sensors_.emplace(cfg.id, std::move(s));
        std::cout << "[SensorManager] loaded sensor " << cfg.id
                  << " port=" << cfg.serial.port << "\n";
    }

    return true;
}
