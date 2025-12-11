#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include "camera_info.h"
#include "plc_info.h"
#include "sensor_types.h"
#include <vector>
struct DeviceStatus
{
    CameraStatusList cameraStatusList;
    PLCList plcStatus_;
    SensorList sensorStatus_;

};

struct RealImage
{
    FrameData frame;
    std::string sourceCameraId;
    bool integrity = false;
};

struct RealImageList
{
    std::vector<RealImage> RealImages;
    bool success = true;
};
struct OperatePLC
{
    std::string deviceId;
    std::string status;
    std::string message;
    bool integrity = false;
};

struct PLCDeviceState
{
    PLCDeviceStatus data;
};

struct RealSensorData
{
    SensorData data;
};

struct OperatePLCWithVerify{
    OperatePLC plcresult;
    RealSensorData sensorData;
    RealImage camData;
    bool isSuccess = false;
}

#endif