#include "plc_manager.h" 
#include <iostream>
#include <algorithm>
#include "serial_plc_connector.h"
#include "solenoid_valve_plc_device.h"
#include "gateway_tcp_connector.h"

// --- PLCManager 实现 ---

PLCManager::PLCManager() : stopThread_(false) {
    // 1. 从 ConfigParser 单例获取配置数据
    DeviceConfigRoot rootConfig = ConfigParser::getInstance().getConfig();

    // 2. 初始化配置映射和连接器
    for(const auto& plcConfig : rootConfig.plcs){
        plcConfigs_[plcConfig.plcId] = plcConfig;
        
        std::unique_ptr<PLCConnector> connector;

        if(plcConfig.connectionType == "direct"){
            connector = std::make_unique<SerialPLCConnector>(plcConfig);
        }
        else if(plcConfig.connectionType == "gateway"){
            connector = std::make_unique<GatewayTCPConnector>(plcConfig);
        }
        else{
            std::cerr << "[PLCManager] WARNING: Unknown connection type '" << plcConfig.connectionType 
                      << "' for PLC ID: " << plcConfig.plcId << "\n";
            continue; // 跳过未知连接类型
        }
        plcConnectors_[plcConfig.plcId] = std::move(connector);
    }

    // 3. 初始化设备配置映射和 PLC-设备关系
    for(const auto& deviceConfig : rootConfig.plcDevices){
        deviceConfigs_[deviceConfig.id] = deviceConfig;
        plcIdToDevices_[deviceConfig.plcId].push_back(deviceConfig);
    }

    if(plcConfigs_.empty()){
        std::cerr << "[PLCManager] WARNING: No PLC configurations found in singleton data.\n";
        // 即使没有 PLC 配置，管理器仍可初始化成功，但功能受限
    }

    // 4. 初始化设备实例
    initializeDevices();

    // 5. 尝试连接所有 PLC
    for (auto const& [plcId, connector] : plcConnectors_) {
        connector->connect();
    }

    // 6. 启动定时刷新线程
    refreshThread_ = std::thread(&PLCManager::periodStatusRefresh, this);
}

PLCManager::~PLCManager() {
    // 断开所有连接
    for (auto const& [plcId, connector] : plcConnectors_) {
        connector->disconnect();
    }
    // 停止定时刷新线程
    stopThread_ = true;
    if (refreshThread_.joinable()) {
        refreshThread_.join();
    }
}

void PLCManager::periodStatusRefresh(){
    while(!stopThread_){
        for(const auto& [plcId, config] : plcConfigs_){
            if(stopThread_) break; // 提前退出
            queryHardwareStatus(plcId);
        }
        std::this_thread::sleep_for(REFRESH_INTERVAL);
    }
}

void PLCManager::initializeDevices(){
    std::cout << "[PLCManager] Initializing devices...\n";
    for(const auto& [deviceId, deviceConfig] : deviceConfigs_){
        // 1.找到对应的PLC连接器
        auto itConnector = plcConnectors_.find(deviceConfig.plcId);
        if(itConnector == plcConnectors_.end()){
            std::cerr << "[PLCManager] ERROR: No connector found for device ID: " << deviceConfig.id << "\n";
            continue;
        }
        PLCConnector* connector = itConnector->second.get();

        // 2.根据deviceType创建设备实例
        std::unique_ptr<IPLCDevice> deviceInstance;
        if(deviceConfig.deviceType == "solenoid_valve"){
            deviceInstance = std::make_unique<SolenoidValvePLCDevice>(deviceConfig, connector);
        }
        else{
            std::cerr << "[PLCManager] WARNING: Unknown device type '" << deviceConfig.deviceType 
                      << "' for device ID: " << deviceConfig.id << "\n";
            continue;
        }
        // 3.存储设备实例（如果需要的话，可以扩展PLCManager以管理设备实例）
        devices_[deviceId] = std::move(deviceInstance);
    }
    std::cout << "[PLCManager] Device initialization complete. Total devices: " << devices_.size() << "\n";
}

bool PLCManager::isCacheExpired(std::chrono::steady_clock::time_point cacheTime) {
    auto now = std::chrono::steady_clock::now();
    return (now - cacheTime) > CACHE_TTL;
}

PLCInfo PLCManager::queryHardwareStatus(const std::string& plcId) {
    PLCInfo currentStatus;
    currentStatus.plcId = plcId;
    currentStatus.lastUpdateTime = getCurrentTimeStr();

    // 1. 查找连接器
    auto itConnector = plcConnectors_.find(plcId);
    if (itConnector == plcConnectors_.end()) {
        currentStatus.connectionStatus = "NOT_FOUND";
        return currentStatus;
    }

    PLCConnector* connector = itConnector->second.get();

    // 获取配置名称
    if(plcConfigs_.count(plcId)) currentStatus.name = plcConfigs_[plcId].name;

    std::lock_guard<std::recursive_mutex> ioLock(connectorMutex_);

    // 2.查询PLC连接状态
    currentStatus.connectionStatus = connector->getConnectionStatus();
    bool isConnected = (currentStatus.connectionStatus == "CONNECTED");

    // 3. 统一遍历设备 (消除重复代码)
    auto itDevices = plcIdToDevices_.find(plcId);
    if (itDevices != plcIdToDevices_.end()) {
        // 小优化：提前预留空间，避免 vector 频繁扩容
        currentStatus.deviceStatuses.reserve(itDevices->second.size()); 

        for (const auto& deviceConfig : itDevices->second) {
            PLCDeviceStatus deviceStatus;
            deviceStatus.id = deviceConfig.id;
            deviceStatus.name = deviceConfig.name;
            deviceStatus.registerAddress = deviceConfig.registerAddress;
            deviceStatus.lastUpdateTime = currentStatus.lastUpdateTime;

            if (isConnected) {
                // 已连接：真实读取
                deviceStatus.status = connector->readRegister(deviceConfig.registerAddress);
            } else {
                // 未连接：设为 UNKNOWN
                deviceStatus.status = "UNKNOWN";
            }
            
            currentStatus.deviceStatuses.push_back(deviceStatus);
        }
    }
    
    // 4. 更新缓存 (需要加锁保护)
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        statusCache_[plcId] = {currentStatus, std::chrono::steady_clock::now()};
    }

    return currentStatus;
}


PLCInfo PLCManager::getStatus(const std::string& deviceId) {
    // 1. 找到deviceId 对应的plcId
    auto itDeviceConfig = deviceConfigs_.find(deviceId);
    if(itDeviceConfig == deviceConfigs_.end()){
        PLCInfo errorInfo;
        errorInfo.plcId = "UNKNOWN";
        errorInfo.connectionStatus = "DEVICE_NOT_FOUND";
        return errorInfo;
    }
    return getPLCStatusInternal(itDeviceConfig->second.plcId);
}

PLCList PLCManager::getAllStatus() {
    PLCList plcList;
    
    for (const auto& pair : plcConfigs_) {
        const std::string& plcId = pair.first;
        plcList.plcList.push_back(getPLCStatusInternal(plcId)); // 直接填充
    }

    return plcList;
}

PLCInfo PLCManager::getPLCStatusInternal(const std::string& plcId){
    // 1.尝试读缓存
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = statusCache_.find(plcId);
        if(it != statusCache_.end() && !isCacheExpired(it->second.lastChecked)){
            // 缓存命中且未过期，直接返回
            std::cout << "[Cache Hit] for PLC: " << plcId << std::endl;
            return it->second.info;
        }
    }

    // 2.缓存失效，执行硬件查询
    std::cout << "[Cache Miss/Expired] Querying PLC: " << plcId << std::endl;
    return queryHardwareStatus(plcId);
}

OperateResult PLCManager::operate(const std::string& deviceId, const std::string& cmd) {
    // 1. 查找设备实例
    auto itDevice = devices_.find(deviceId);
    if(itDevice == devices_.end()){
        OperateResult result;
        result.success = false;
        result.message = "Device ID not found: " + deviceId;
        return result;
    }
    IPLCDevice* device = itDevice->second.get();
    OperateResult result;

    // 2. 执行操作
    {
        std::lock_guard<std::recursive_mutex> ioLock(connectorMutex_);
        result = device->writeControl(cmd);
    }
    
    // 3. 操作成功后，立即清空该 PLC 的状态缓存。
    if (result.success) {
        // 查找设备配置以获取 PLC ID
        auto itConfig = deviceConfigs_.find(deviceId);
        if (itConfig != deviceConfigs_.end()) {
             std::lock_guard<std::mutex> lock(cacheMutex_);
             statusCache_.erase(itConfig->second.plcId);
        } else {
             std::cerr << "[PLCManager] WARNING: Could not find config for device ID " << deviceId << " to clear cache.\n";
        }
    }

    return result;
}