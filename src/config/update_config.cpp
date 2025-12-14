#include "update_config.h"
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

static const std::string TEMP_FILE_PATH = "/home/ztl/workspace/allin/include/common/config/temp.json";
static const std::string CONFIG_FILE_PATH = "/home/ztl/workspace/allin/include/common/config/config.json";

// 写入temp.json
bool ConfigUpdater::writeToTempFile(const std::string& jsonMessage){
    std::ofstream tempFile(TEMP_FILE_PATH);
    if(!tempFile.is_open()){
        std::cerr<<"[UpdateConfig] Could not open temporary file for writing: "<<TEMP_FILE_PATH<<std::endl;
        return false;
    }

    try{
        tempFile << jsonMessage;
        tempFile.flush();
        tempFile.close();

        if (tempFile.fail()) {
             std::cerr << "Error: I/O operation failed during file closing or writing." << std::endl;
             return false;
        }
        return true;
    } catch(const std::exception& e){
        std::cerr << "An unexpected error occurred during file writing: " << e.what() << std::endl;
        if (tempFile.is_open()) tempFile.close();
        return false;
    }
}

// 校验JSON结构
bool ConfigUpdater::validateJsonFormat(json& outConfig){
    try{
        std::ifstream ifs(TEMP_FILE_PATH);
        ifs>>outConfig;

        if(!outConfig.contains("device_config") || !outConfig.at("device_config").is_object()){
            std::cerr << "Validation failed: Missing or invalid 'device_config' root." << std::endl;
            return false;
        }
        if(!outConfig.at("device_config").contains("devices") || !outConfig.at("device_config").at("devices").is_object()){
            std::cerr << "Validation failed: Missing or invalid 'devices' field." << std::endl;
            return false;
        }

        return true;
    }catch(const json::parse_error& e){
        std::cerr << "JSON Parse Error: " << e.what() << " at byte " << e.byte << std::endl;
        return false;
    }catch (const std::exception& e) {
        std::cerr << "File I/O Error during JSON loading: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigUpdater::checkAndAddId(
    const json& item,
    const std::string& id_field,
    const std::string& item_type,
    std::set<std::string>& allDeviceIds,
    std::string& errorMessage) 
{
    // 检查 ID 字段是否存在、是字符串且非空
    if (!item.contains(id_field) || !item.at(id_field).is_string() || item.at(id_field).empty()) {
        errorMessage = item_type + " validation failed: Missing or invalid '" + id_field + "' field.";
        return false;
    }
    
    std::string currentId = item.at(id_field).get<std::string>();
    
    // 检查 ID 是否重复
    if (allDeviceIds.count(currentId)) {
        errorMessage = "Device ID '" + currentId + "' is duplicated across different device types or within the same list.";
        return false;
    }
    
    allDeviceIds.insert(currentId);
    return true;
}

bool ConfigUpdater::validateBusinessLogic(const json& newConfig, std::string& errorMessage) {
    try {
        const json& deviceConfig = newConfig.at("device_config");
        const json& devices = deviceConfig.at("devices");

        // --- 0. 全局 ID 唯一性检查初始化 ---
        std::set<std::string> allDeviceIds;
        std::string currentId; // 用于在各个循环中存储当前设备的ID，方便报错

        // --- 1. 摄像头 (camera) 校验 ---
        if (devices.contains("camera") && devices.at("camera").is_array()) {
            for (const auto& cam : devices.at("camera")) {
                if (!checkAndAddId(cam, "id", "Camera", allDeviceIds, errorMessage)) return false; 
                currentId = cam.at("id").get<std::string>(); // 更新 ID 供后续使用
                
                // URL 格式校验
                if (!cam.contains("url") || !cam.at("url").is_string()) {
                    errorMessage = "Camera (" + currentId + ") has invalid or missing 'rtsp://' URL.";
                    return false;
                }
            }
        }

        // --- 2. PLC 列表 (plc_list) 校验 ---
        std::set<std::string> validPlcIds; // 用于 PLC Device 关联检查
        if (devices.contains("plc_list") && devices.at("plc_list").is_array()) {
            for (const auto& plc : devices.at("plc_list")) { 
                if (!checkAndAddId(plc, "plc_id", "PLC", allDeviceIds, errorMessage)) return false;
                currentId = plc.at("plc_id").get<std::string>();
                validPlcIds.insert(currentId);
                
                if (!plc.contains("slave_id") || !plc.at("slave_id").is_string() || plc.at("slave_id").empty()) {
                    errorMessage = "PLC (" + currentId + "): Missing or invalid 'slave_id'.";
                    return false;
                }

                // 逻辑校验：检查连接类型依赖
                std::string connType = plc.at("connection_type").get<std::string>();
                if (connType == "direct") {
                    if (!plc.contains("serial")) {
                        errorMessage = "PLC (" + currentId + "): Direct connection missing 'serial' config.";
                        return false;
                    }
                    if (!plc.at("serial").contains("baud_rate") || !plc.at("serial").at("baud_rate").is_number_integer()) {
                         errorMessage = "PLC (" + currentId + "): Baud rate must be an integer.";
                         return false;
                    }
                } else if (connType == "gateway") {
                    if (!plc.contains("gateway")) {
                        errorMessage = "PLC (" + currentId + "): Gateway connection missing 'gateway' config.";
                        return false;
                    }
                    if (!plc.at("gateway").contains("gateway_port") || !plc.at("gateway").at("gateway_port").is_number_integer()) {
                         errorMessage = "PLC (" + currentId + "): Gateway port must be an integer.";
                         return false;
                    }
                } else {
                    errorMessage = "PLC (" + currentId + "): Invalid 'connection_type'. Must be 'direct' or 'gateway'.";
                    return false;
                }
            }
        }

        // --- 3. PLC 设备 (plc_device) 关联性校验 ---
        if (devices.contains("plc_device") && devices.at("plc_device").is_array()) {
            for (const auto& dev : devices.at("plc_device")) {
                if (!checkAndAddId(dev, "id", "PLCDevice", allDeviceIds, errorMessage)) return false;
                currentId = dev.at("id").get<std::string>();
                
                // 关联性校验
                if (!dev.contains("plc_id") || !dev.at("plc_id").is_string()) {
                    errorMessage = "PLCDevice (" + currentId + ") missing 'plc_id' field.";
                    return false;
                }
                std::string refPlcId = dev.at("plc_id").get<std::string>();
                
                if (validPlcIds.find(refPlcId) == validPlcIds.end()) {
                    errorMessage = "PLCDevice ('" + currentId + "') refers to unknown plc_id '" + refPlcId + "'.";
                    return false;
                }
            }
        }
        
        // --- 4. 传感器 (sensor) 校验 ---
        if (devices.contains("sensor") && devices.at("sensor").is_array()) {
            for (const auto& sensor : devices.at("sensor")) {
                if (!checkAndAddId(sensor, "id", "Sensor", allDeviceIds, errorMessage)) return false;
                currentId = sensor.at("id").get<std::string>();
                
                // 校验类型和依赖
                if (!sensor.contains("type") || !sensor.at("type").is_string()) {
                    errorMessage = "Sensor (" + currentId + "): Missing or invalid 'type'.";
                    return false;
                }
                std::string sensorType = sensor.at("type").get<std::string>();
                
                if (sensorType == "modbus") {
                    if (!sensor.contains("serial_config")) {
                        errorMessage = "Sensor (" + currentId + "): Modbus type missing 'serial_config'.";
                        return false;
                    }
                    if (!sensor.at("reg_count").is_number_integer() || sensor.at("reg_count").get<int>() <= 0) {
                        errorMessage = "Sensor (" + currentId + "): Modbus 'reg_count' must be a positive integer.";
                        return false;
                    }
                }
                // ... 其他传感器类型校验 ...
            }
        }

        // --- 5. 网关 (gateway) 校验 ---
        if (devices.contains("gateway") && devices.at("gateway").is_array()) {
            for (const auto& gateway : devices.at("gateway")) {
                if (!checkAndAddId(gateway, "id", "Gateway", allDeviceIds, errorMessage)) return false;
                currentId = gateway.at("id").get<std::string>();
                
                // IP 和 Protocol 校验
                if (!gateway.contains("ip") || !gateway.at("ip").is_string() || gateway.at("ip").empty()) {
                    errorMessage = "Gateway (" + currentId + "): Missing or invalid 'ip' address.";
                    return false;
                }
                if (!gateway.contains("protocol") || !gateway.at("protocol").is_string() || gateway.at("protocol").empty()) {
                     errorMessage = "Gateway (" + currentId + "): Missing or invalid 'protocol'.";
                    return false;
                }
            }
        }
        
        // --- 6. CarControl 校验 ---
        if (devices.contains("carcontrol") && devices.at("carcontrol").is_array()) {
            for (const auto& cc : devices.at("carcontrol")) {
                if (!checkAndAddId(cc, "id", "CarControl", allDeviceIds, errorMessage)) return false;
                currentId = cc.at("id").get<std::string>();
                // ... CarControl 自身的业务逻辑校验 ...
            }
        }

        return true; 

    } catch (const json::out_of_range& e) {
        errorMessage = "Business logic validation failed due to missing critical field: " + std::string(e.what());
        return false;
    } catch (const std::exception& e) {
        errorMessage = "An unexpected error occurred during business validation: " + std::string(e.what());
        return false;
    }
}

UpdateConfigResult ConfigUpdater::updateConfig(const std::string& receivedJsonMessage){
    UpdateConfigResult result;
    json newConfig;

    // 1.写入临时文件
    if (!writeToTempFile(receivedJsonMessage)) {
        result.message = "Failed to write JSON message to temporary file.";
        return result;
    }

    // 2. JSON 格式和基础结构校验 (从文件加载并解析)
    if (!validateJsonFormat(newConfig)) {
        result.message = "Temporary file is not a valid JSON or lacks root structure.";
        std::remove(TEMP_FILE_PATH.c_str()); // 清理
        return result;
    }

    // 3. 业务逻辑校验
    if (!validateBusinessLogic(newConfig, result.message)) {
        // validateBusinessLogic 已经填充了详细的错误信息
        result.message = "Business logic validation failed: " + result.message;
        std::remove(TEMP_FILE_PATH.c_str()); // 清理
        return result;
    }

    // 4. 校验成功：原子覆盖原文件
    if (std::rename(TEMP_FILE_PATH.c_str(), CONFIG_FILE_PATH.c_str()) != 0) {
        // rename 失败，原 config.json 仍存在，temp.json 应该还存在（未被移动）
        result.message = "Configuration update failed at atomic replacement (std::rename failed). Original config untouched.";
        // 此时为了保险，删除 temp.json
        std::remove(TEMP_FILE_PATH.c_str());
        return result;
    }
    // 5. 成功
    result.isSuccess = true;
    result.message = "Configuration successfully updated and validated.";
    return result;
}