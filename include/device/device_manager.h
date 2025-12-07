#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "icamera_manager.h"
// #include "isensor_manager.h"
#include "idevice_manager.h"

class DeviceManager : public IDeviceManager{
public:
    DeviceManager();
    ~DeviceManager();
    DeviceStatus getStatus() override;

    void getAllRealImage() override;
    RealImage getRealImage(const std::string& camId) override;
    void getAllHistoryImage() override;
    void getHistoryImage(const std::string& camId) override;
    
    void operateCamera() override;
    void operatePlc(const std::string &deviceId, const std::string &cmd) override;
    
    void updateConfig() override;

private:
    std::shared_ptr<ICameraManager> cameraManager_;
    // std::shared_ptr<IPLCManager> plcManager_;
    // std::shared_ptr<ISensorManager> sensorManager_;

};

#endif