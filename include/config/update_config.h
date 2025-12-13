#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <set>
#include "json.hpp"
#include "device_info.h"

class ConfigUpdater{
public:
    ConfigUpdater() = default;
    UpdateConfigResult updateConfig(const std::string& receivedJsonMessage);
private:
    bool writeToTempFile(const std::string& jsonMessage);
    bool validateJsonFormat(nlohmann::json& outConfig);
    bool validateBusinessLogic(const nlohmann::json& newConfig, std::string& errorMessage);
    bool checkAndAddId(
        const nlohmann::json& item,
        const std::string& id_field,
        const std::string& item_type,
        std::set<std::string>& allDeviceIds,
        std::string& errorMessage
    );
};