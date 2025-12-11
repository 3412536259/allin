#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include "config_info.h"
#include "iplc_manager.h"
#include "plc_connector.h" // 包含 PLCConnector 和 MockPLCConnector
#include "config_parser.h"
#include "iplc_device.h"
#include "base_plc_device.h"


// 状态缓存结构体，包含时间戳
struct PLCStatusCache {
    PLCInfo info;
    std::chrono::steady_clock::time_point lastChecked;
};

class PLCManager : public IPLCManager {
public:
    PLCManager();
    ~PLCManager() override;

    PLCInfo getStatus(const std::string& deviceId) override;
    PLCList getAllStatus() override;
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
    // ---------------- 设备实例 ----------------
    // <设备 ID, 设备实例>
    std::map<std::string, std::unique_ptr<IPLCDevice>> devices_;

    // ---------------- 状态缓存 ----------------
    // <PLC ID, 状态缓存>
    std::map<std::string, PLCStatusCache> statusCache_; 
    // 保护状态缓存的互斥锁
    std::mutex cacheMutex_; 
    // 保护Connector IO 操作的互斥锁，放置后台刷新和前台控制同时写入同一个socket/串口
    std::recursive_mutex connectorMutex_;

    PLCInfo getPLCStatusInternal(const std::string& plcId);

    // 状态缓存的有效期 (例如 2 秒)
    const std::chrono::seconds CACHE_TTL = std::chrono::seconds(8); 

    // 定时刷新线程
    std::atomic<bool> stopThread_{false};
    std::thread refreshThread_;
    void periodStatusRefresh();     // 定时刷新函数
    const std::chrono::seconds REFRESH_INTERVAL = std::chrono::seconds(5); // 刷新间隔

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
    PLCInfo queryHardwareStatus(const std::string& plcId);
    /**
     * @brief 初始化所有设备实例，根据 deviceType 创建对象
     */
    void initializeDevices();
};