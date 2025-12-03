#include "plc_manager.h" 
#include <iostream>
#include <algorithm>

// --- 实用工具函数 ---

// 获取当前时间字符串
std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};

    std::tm* result = std::localtime(&in_time_t);
    if (result) {
        bt = *result;
    } else {
        // 错误处理，返回空或默认时间
        return "Time Error"; 
    }

    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S"); // **[修正]**：std::put_time 需要 <iomanip>
    return ss.str();
}

// --- PLCManager 实现 ---

PLCManager::PLCManager() {
    // 1. 初始化
    if(!initialize()){
        std::cerr << "[PLCManager] Initialization failed.\n";
    }

    // 2. 尝试连接所有 PLC
    for (auto const& [plcId, connector] : plcConnectors_) {
        connector->connect();
    }
}

PLCManager::~PLCManager() {
    // 断开所有连接
    for (auto const& [plcId, connector] : plcConnectors_) {
        connector->disconnect();
    }
}

bool PLCManager::initialize(){
    ConfigParser parser;
    const std::string CONFIG_FILE_PATH = "../include/common/config/config.json";
    if(!parser.loadFromFile(CONFIG_FILE_PATH)){
        std::cerr << "[PLCManager] Failed to load config from " << CONFIG_FILE_PATH << "\n";
        return false;
    }

    DeviceConfigRoot rootConfig = parser.getConfig();

    // 1. 初始化配置映射
    for(const auto& plcConfig : rootConfig.plcs){
        plcConfigs_[plcConfig.plcId] = plcConfig;
        plcConnectors_[plcConfig.plcId] = std::make_unique<MockPLCConnector>(plcConfig);
    }

    // 2. 初始化设备配置映射和 PLC-设备关系
    for(const auto& deviceConfig : rootConfig.plcDevices){
        deviceConfigs_[deviceConfig.id] = deviceConfig;
        plcIdToDevices_[deviceConfig.plcId].push_back(deviceConfig);
    }

    if(plcConfigs_.empty()){
        std::cerr << "[PLCManager] No PLC configurations found.\n";
        return false;
    }

    return true;
}

bool PLCManager::isCacheExpired(std::chrono::steady_clock::time_point cacheTime) {
    auto now = std::chrono::steady_clock::now();
    return (now - cacheTime) > CACHE_TTL;
}

PLCInfo PLCManager::queryAndRefreshStatus(const std::string& plcId) {
    auto itConnector = plcConnectors_.find(plcId);
    if (itConnector == plcConnectors_.end()) {
        PLCInfo errorInfo;
        errorInfo.plcId = plcId;
        errorInfo.connectionStatus = "NOT_FOUND";
        return errorInfo;
    }

    PLCInfo currentStatus;
    PLCConnector* connector = itConnector->second.get();

    // 1. 查询 PLC 本体状态
    currentStatus.plcId = plcId;
    currentStatus.name = plcConfigs_[plcId].name;
    currentStatus.connectionStatus = connector->getConnectionStatus();
    currentStatus.lastUpdateTime = getCurrentTimeStr();

    // 2. 查询下挂设备状态
    if (currentStatus.connectionStatus == "CONNECTED") {
        auto itDevices = plcIdToDevices_.find(plcId);
        if (itDevices != plcIdToDevices_.end()) {
            for (const auto& deviceConfig : itDevices->second) {
                PLCDeviceStatus deviceStatus;
                deviceStatus.id = deviceConfig.id;
                deviceStatus.name = deviceConfig.name;
                deviceStatus.registerAddress = deviceConfig.registerAddress;
                deviceStatus.status = connector->readRegister(deviceConfig.registerAddress); // 实时读取寄存器
                deviceStatus.lastUpdateTime = currentStatus.lastUpdateTime;
                currentStatus.deviceStatuses.push_back(deviceStatus);
            }
        }
    } else {
        // 如果 PLC 未连接，则所有下挂设备状态为 UNKNOWN
        auto itDevices = plcIdToDevices_.find(plcId);
        if (itDevices != plcIdToDevices_.end()) {
             for (const auto& deviceConfig : itDevices->second) {
                PLCDeviceStatus deviceStatus;
                deviceStatus.id = deviceConfig.id;
                deviceStatus.name = deviceConfig.name;
                deviceStatus.registerAddress = deviceConfig.registerAddress;
                deviceStatus.status = "UNKNOWN"; 
                deviceStatus.lastUpdateTime = currentStatus.lastUpdateTime;
                currentStatus.deviceStatuses.push_back(deviceStatus);
             }
        }
    }
    
    // 3. 更新缓存 (需要加锁保护)
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        statusCache_[plcId] = {currentStatus, std::chrono::steady_clock::now()};
    }

    return currentStatus;
}


PLCInfo PLCManager::getStatus(const std::string& plcId) {
    // 1. 尝试从缓存读取 (需要加锁保护)
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = statusCache_.find(plcId);
        if (it != statusCache_.end() && !isCacheExpired(it->second.lastChecked)) {
            // 缓存命中且未过期，直接返回
            // std::cout << "[Cache Hit] for PLC: " << plcId << std::endl;
            return it->second.info;
        }
        // 缓存未命中或过期
    }
    
    // 2. 缓存失效，实时查询并更新缓存
    // std::cout << "[Cache Miss/Expired] Querying PLC: " << plcId << std::endl;
    return queryAndRefreshStatus(plcId);
}

std::vector<PLCInfo> PLCManager::getAllStatus() {
    std::vector<PLCInfo> allStatus;
    
    // 遍历所有 PLC ID，逐个调用 getStatus
    for (const auto& pair : plcConfigs_) {
        // getStatus 内部会处理缓存逻辑
        allStatus.push_back(getStatus(pair.first)); 
    }

    return allStatus;
}

OperateResult PLCManager::operate(const std::string& deviceId, const std::string& cmd) {
    OperateResult result;
    result.success = false;

    // 1. 查找设备配置
    auto itDevice = deviceConfigs_.find(deviceId);
    if (itDevice == deviceConfigs_.end()) {
        result.message = "Device ID not found: " + deviceId;
        return result;
    }
    const PLCDeviceConfig& deviceConfig = itDevice->second;

    // 2. 查找 PLC 连接器
    auto itConnector = plcConnectors_.find(deviceConfig.plcId);
    if (itConnector == plcConnectors_.end()) {
        result.message = "PLC connector not found for device: " + deviceId;
        return result;
    }
    PLCConnector* connector = itConnector->second.get();

    // 3. 检查 PLC 连接状态
    if (connector->getConnectionStatus() != "CONNECTED") {
        result.message = "PLC " + deviceConfig.plcId + " is not connected.";
        return result;
    }
    
    // 4. 将命令转换为寄存器值 (此处进行简化)
    std::string value;
    if (cmd == "ON") {
        value = "1";
    } else if (cmd == "OFF") {
        value = "0";
    } else {
        result.message = "Invalid command: " + cmd + ". Must be ON or OFF.";
        return result;
    }

    // 5. 执行写入操作
    if (connector->writeRegister(deviceConfig.registerAddress, value)) {
        result.success = true;
        result.message = "Operate successfully: " + deviceId + " set to " + cmd;

        // 【重要】操作成功后，需要立即清空该 PLC 的状态缓存，以保证下次查询是实时结果。
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            statusCache_.erase(deviceConfig.plcId);
        }
        
    } else {
        result.message = "Failed to write to PLC register for device: " + deviceId;
    }

    return result;
}