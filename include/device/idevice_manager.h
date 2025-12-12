#ifndef I_DEVICE_MANAGER_H
#define I_DEVICE_MANAGER_H
#include <string>
#include "camera_info.h"
#include "device_info.h"
class IDeviceManager{
public:
    virtual ~IDeviceManager() = default;

    virtual DeviceStatus getStatus() = 0; //设备状态获取

    virtual RealImage getRealImage(const std::string& camId) = 0; //获取对应摄像头实时图片
    virtual RealImageList getAllRealImage() = 0; //获取所有摄像头实时图片
    virtual void getHistoryImage(const std::string& camId) = 0;
    virtual void getAllHistoryImage() = 0;
    
    virtual void operateCamera() = 0;
    virtual OperatePLC operatePlc(const std::string &deviceId, const std::string &cmd) = 0;
    // virtual OperatePLCWithVerify operatePlcWithVerify(const std::string& deviceId, const std::string& cmd, const std::string& sensorId, const std::string& cameraId);
    virtual PLCDeviceState getPLCDeviceStatus(const std::string& deviceId) = 0;
    virtual RealSensorData getSensorData(const std::string& sensorId) = 0;
    virtual CarControlResult operateCarControl(const std::string& carControlId, int motor1, int motor2) = 0;

    virtual void updateConfig() = 0; //更新配置

};



#endif