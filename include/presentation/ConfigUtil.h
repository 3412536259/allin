#pragma once

#include <string>
#include "json.hpp"

using nlohmann::json;



class ConfigUtil {
public:

    static bool loadBoxId(const std::string& path, std::string& outBoxId);

    static const std::string& getConfigPath();
};


