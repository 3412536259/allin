#pragma once

#include <string>
#include "config_info.h"
#include "json.hpp"   // nlohmann/json

class ConfigParser {
public:
    bool loadFromFile(const std::string& path);
    const DeviceConfigRoot& getConfig() const { return config_; }

private:
    void parseCameras(const nlohmann::json& j);
    void parsePLCDevices(const nlohmann::json& j);
    void parseSensors(const nlohmann::json& j);
    void parseGateways(const nlohmann::json& j);

private:
    DeviceConfigRoot config_;
};
