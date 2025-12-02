#ifndef I_DEVICE_MANAGER_H
#define I_DEVICE_MANAGER_H
#include <string>
#include "camera_info.h"
#include "device_info.h"
class IDeviceManager{
public:
    virtual ~IDeviceManager() = default;

    virtual DeviceStatus getStatus() = 0;

    virtual void getRealImage(const CameraStaticInfo& info) = 0;
    virtual void getAllRealImage() = 0;
    virtual void getHistoryImage(const CameraStaticInfo& info) = 0;
    virtual void getAllHistoryImage() = 0;
    
    virtual void operateCamera() = 0;
    virtual void operatePlc(const std::string &deviceId, const std::string &cmd) = 0;

    virtual void updateConfig() = 0;

};



#endif