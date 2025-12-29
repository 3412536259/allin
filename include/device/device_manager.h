#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "icamera_manager.h"
#include "isensor_manager.h"
#include "idevice_manager.h"
#include "update_config.h"
#include "iplc_manager.h"
#include "icar_control_manager.h"
#include "device_info.h"

class ICarControlManager;
class DeviceManager : public IDeviceManager{
public:
    DeviceManager();
    ~DeviceManager();
    DeviceStatus getStatus() override;

    RealImageList getAllRealImage() override;
    RealImage getRealImage(const std::string& camId) override;
    void getAllHistoryImage() override;
    void getHistoryImage(const std::string& camId) override;
    
    void operateCamera() override;
    OperatePLC operatePlc(const std::string &deviceId, const std::string &cmd) override;
    OperatePLCWithVerify operatePlcWithVerify(const std::string& deviceId, const std::string& cmd, const std::string& sensorId, const std::string& cameraId) override;
    PLCDeviceState getPLCDeviceStatus(const std::string& deviceId) override;
    RealSensorData getSensorData(const std::string& sensorId) override;
    CarControlResult operateCarControl(const std::string& carControlId, int motor1, int motor2) override;

    // request cancellation/interruption of any currently running car control operation for the given id
    void cancelCarControl(const std::string& carControlId);

    // query last observed status for car control device; returns -1 if no reply observed
    int getCarControlLastStatus(const std::string& carControlId);

    UpdateConfigResult configUpdate(const std::string& JsonStr) override;

private:
    std::shared_ptr<ICameraManager> cameraManager_;
    std::shared_ptr<IPLCManager> plcManager_;
    std::shared_ptr<ISensorManager> sensorManager_;
    std::shared_ptr<ICarControlManager> carControlManager_;
    std::shared_ptr<ConfigUpdater> configUpdater_;
};

#endif