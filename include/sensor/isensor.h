#ifndef I_SENSOR_H
#define I_SENSOR_H

#include "config_info.h"
#include "sensor_types.h"
#include <string>
#include <optional>
#include <memory>
#include <iostream>
#include <ostream>

// 抽象传感器接口
struct ISensor {
    virtual ~ISensor() = default;

    // 使用 config 初始化（打开串口等）
    virtual bool init() = 0;

    // 关闭串口/释放资源
    virtual void closeSerial() = 0;

    // 从硬件读取并更新内部缓存（阻塞）
    // 返回 true 表示读取成功
    virtual bool readData() = 0;

    // 下面的 getter 不应触发 IO，仅返回内部缓存
    virtual std::string getId() const = 0;
    virtual std::string getType() const = 0; // 新增：获取传感器类型
    virtual float getTemperatureC() const = 0;
    virtual float getHumidityPct() const = 0;
    virtual float getValue() const = 0; 
    virtual SensorStatus getStatus() const = 0;
};

// 传感器工厂
std::unique_ptr<ISensor> createSensor(const SensorConfig& config);

#endif // I_SENSOR_H