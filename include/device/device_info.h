#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include "camera_info.h"
// #include "plc_info.h"
#include "sensor_types.h"
#include <vector>
struct DeviceStatus
{
    std::vector<CameraStatus> cameraStatus_;
    // std::vector<PLCInfo> plcStatus_;
    std::vector<SensorData> sensorStatus_;

};

struct RealImage
{
    FrameData frame;
    std::string sourceCameraId;
    bool integrity = false;
};

#endif