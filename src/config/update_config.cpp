#include "update_config.h"

using json = nlohmann::json;

static const std::string TEMP_FILE = "temp.json";
static const std::string CONFIG_FILE = "config.json";

// 写入temp.json
bool writeTempConfig(const std::string& jsonStr){
    try{
        std::ofstream ofs(TEMP_FILE);
        if(!ofs) return false;
        ofs<<jsonStr;
        ofs.close();
        return true;
    } catch(...){
        return false;
    }
}

// 校验JSON结构
bool validateTempConfig(const json& j){
    if(!j.contains("device_config")) return false;
    if(!j["device_config"].contains("devices")) return false;

    const auto& devices = j["device_config"]["devices"];

    
}