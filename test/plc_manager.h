#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include "config_info.h"
#include "iplc_manager.h"
#include "plc_connector.h" // 包含 PLCConnector 和 MockPLCConnector
#include "config_parser.h"
#include "mock_plc_connector.h"

// 状态缓存结构体，包含时间戳
struct PLCStatusCache {
    PLCInfo info;
    std::chrono::steady_clock::time_point lastChecked;
};

class PLCManager : public IPLCManager {
public:
    PLCManager();
    ~PLCManager() override;

    PLCInfo getStatus(const std::string& plcId) override;
    std::vector<PLCInfo> getAllStatus() override;
    OperateResult operate(const std::string& deviceId, const std::string& cmd) override;

private:
    // ---------------- 配置数据 ----------------
    // <PLC ID, PLC 配置>
    std::map<std::string, PLCConfig> plcConfigs_; 
    // <设备 ID, 设备配置>
    std::map<std::string, PLCDeviceConfig> deviceConfigs_; 
    // <PLC ID, 归属于该 PLC 的设备列表>
    std::map<std::string, std::vector<PLCDeviceConfig>> plcIdToDevices_; 

    // ---------------- PLC 连接器 ----------------
    // <PLC ID, PLC 连接器实例>
    std::map<std::string, std::unique_ptr<PLCConnector>> plcConnectors_; 

    // ---------------- 状态缓存 ----------------
    // <PLC ID, 状态缓存>
    std::map<std::string, PLCStatusCache> statusCache_; 
    // 保护状态缓存的互斥锁
    std::mutex cacheMutex_; 

    // 状态缓存的有效期 (例如 2 秒)
    const std::chrono::seconds CACHE_TTL = std::chrono::seconds(2); 

    /**
     * @brief 检查缓存是否过期
     * @param cacheTime 缓存时间点
     * @return 是否过期
     */
    bool isCacheExpired(std::chrono::steady_clock::time_point cacheTime);

    /**
     * @brief 从 PLC 实时查询状态并更新缓存
     * @param plcId PLC ID
     * @return 实时查询到的状态
     */
    PLCInfo queryAndRefreshStatus(const std::string& plcId);
    /**
     * @brief 内部函数：从文件加载配置并初始化内部结构。
     * @return bool 成功加载返回 true，否则返回 false。
     */
    bool initialize();
};