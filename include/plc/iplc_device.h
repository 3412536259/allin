#pragma once

#include <string>
#include <memory>
#include "config_info.h"
#include "plc_info.h"
#include "plc_connector.h"

// 统一的下挂设备操作接口
class IPLCDevice{
public:
    virtual ~IPLCDevice() = default;

    /**
     * @brief 读取设备状态
     * @return 设备状态字符串（例如 "ON" 或 "OFF"）
     */
    virtual std::string readStatus() = 0;

    /**
     * @brief 操作设备，写入控制命令
     * @param value 要写入的状态值（例如 "ON" 或 "OFF" 或"1/0"）
     * @return 操作是否成功
     */
    virtual OperateResult writeControl(const std::string& cmd) = 0;

    /**
     * @brief 获取设备ID
     * @return 设备ID字符串
     */
    virtual const std::string getDeviceId() const = 0;
};