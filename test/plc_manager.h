#pragma once

#include "config_info.h"
#include "plc_info.h"
#include "iplc_connector.h"
#include "iplc_device.h"


#include <string>
#include <vector>
#include <map>
#include <memory>

class PLCManager : public IPLCManager{
public:
    PLCManager();
    ~PLCManager() override;

    PLCInfo getStatus(const std::string& deviceId) override;
    std::vector<PLCInfo> getAllStatus() override;
    OperateResult operate(const std::string& deviceId, const std::string& cmd) override;

private:
    // PLC ID - PLC 配置
    std::map<std::string, PLCConfig> plcConfigs_;
    // 设备 ID - 设备配置
    std::map<std::string, PLCDeviceConfig> deviceConfigs_;
    // PLC ID - 属于 PLC 的下挂设备列表
    std::map<std::string, std::vector<PLCDeviceConfig>> plcIdToDevices_;

    // PLC ID - PLC连接器实例
    std::map<std::string, std::unique_ptr<IPLCConnector>> plcConnectors_;
    // 设备 ID - 设备实例
    std::map<std::string, std::unique_ptr<IPLCDevice>> devices_;
    /**
     * @brief 从 PLC 实时查询状态并更新缓存
     * @param plcId PLC ID
     * @return 实时查询到的状态
     */
    PLCInfo queryAndRefreshStatus(const std::string& plcId);
};