#include "ConfigUtil.h"
#include <fstream>
#include <iostream>

using nlohmann::json;
static const std::string CONFIG_PATH = "/home/ztl/workspace/allin/include/common/config/config.json";

bool ConfigUtil::loadBoxId(const std::string& path, std::string& outBoxId)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "ConfigUtil: failed to open " << path << "\n";
        return false;
    }
    try {
        json j;
        ifs >> j;
        if (j.contains("box_id")) {
            outBoxId = j["box_id"].get<std::string>();
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "ConfigUtil: json parse error: " << e.what() << "\n";
        return false;
    }
    return false;
}

const std::string& ConfigUtil::getConfigPath() {
    return CONFIG_PATH;
}