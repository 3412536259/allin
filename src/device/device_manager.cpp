#include "device_manager.h"
#include "camera_manager.h"
#include "sensor_manager.h"
#include "plc_manager.h"
#include "car_control_manager.h"
#include <iostream>
DeviceManager::DeviceManager()
{
    cameraManager_ = std::make_shared<CameraManager>();
    sensorManager_ = std::make_shared<SensorManager>();
    plcManager_ = std::make_shared<PLCManager>();
    carControlManager_ = std::make_shared<CarControlManager>();
}

DeviceManager::~DeviceManager()
{}

DeviceStatus DeviceManager::getStatus()
{
    DeviceStatus deviceStatus;
    deviceStatus.cameraStatusList = cameraManager_->getAllStatus();
    if(plcManager_){
        PLCList plcList = plcManager_->getAllStatus();
        deviceStatus.plcStatus_ = plcList;
    }
    
    if(sensorManager_){
        deviceStatus.sensorStatus_ = sensorManager_ -> getAllSensorData();
    }

    return deviceStatus;
}

RealImageList DeviceManager::getAllRealImage()
{
    RealImageList list;
    RealImage image;
    if(!cameraManager_)
    {
        list.success = false;
        return list;
    }
    auto allFrames = cameraManager_->getAllLastKeyFrames();

    for(auto& kv : allFrames)
    {
        std::string id = kv.first;
        const FrameData& frame = kv.second;
        image.integrity = true;
        image.frame = frame;
        image.sourceCameraId = id;
        list.RealImages.push_back(image);
        // TODO: 上传云端或回调 UI
        // cloudUploader.uploadRealImage(id, frame);
 
    }
    return list;
}

RealImage DeviceManager::getRealImage(const std::string& camId)
{
    RealImage realImage;
    if (!cameraManager_) {
        std::cerr << "DeviceManager: cameraManager_ is null!" << std::endl;
        return realImage;
    }
    CameraStaticInfo info;
    info.camera_id = camId;
    FrameData frame;
    
    realImage.sourceCameraId = camId;
    // 调用 CameraManager 获取关键帧
    bool ok = cameraManager_->getCameraLastKeyFrame(info, frame);
    if (!ok) {
        std::cerr << "DeviceManager: failed to get real image for camera "<< info.camera_id << std::endl;
        return realImage;
    }
    realImage.frame = frame;
    realImage.integrity = true;
    // TODO：把 frame 传递到云端 或者回调给上层
    // 示例（你之后自己替换上传函数）：
    // cloudUploader_.uploadRealImage(id, frame);

    std::cout << "DeviceManager: Real image retrieved for camera " 
              << info.camera_id << ", timestamp=" << frame.timestamp << std::endl;

    return realImage;
}

void DeviceManager::getAllHistoryImage()
{

}

void DeviceManager::getHistoryImage(const std::string& camId)
{

}
void DeviceManager::operateCamera()
{

}
OperatePLC DeviceManager::operatePlc(const std::string &deviceId, const std::string &cmd)
{
    OperatePLC result;
    if(!plcManager_){
        std::cerr << "DeviceManager: plcManager is null!"<<std::endl;
        return result;
    }
    OperateResult res = plcManager_->operate(deviceId,cmd);
    result.deviceId = deviceId;
    result.integrity = res.success;
    result.message = res.message;
    PLCInfo status = plcManager_->getStatus(deviceId);
    auto itDeviceStatus = std::find_if(status.deviceStatuses.begin(), status.deviceStatuses.end(), 
                                           [&deviceId](const PLCDeviceStatus& ds) {
                                               return ds.id == deviceId;
                                           });
    if(itDeviceStatus != status.deviceStatuses.end()){
        result.status = itDeviceStatus->status;
    }
    else{
        result.status = "UNKNOWN";
    }
    
    return result;

    // TODO：把 res 传递到云端 或者回调给上层
    // 示例（你之后自己替换上传函数）：
    // cloudUploader_.uploadRealImage(deviceId, res);
}

OperatePLCWithVerify DeviceManager::operatePlcWithVerify(const std::string& deviceId, const std::string& cmd, const std::string& sensorId, const std::string& cameraId){
    OperatePLCWithVerify result;
    if(!plcManager_){
        std::cerr << "DeviceManager: plcManager is null!"<<std::endl;
        return result;
    }
    OperateResult res = plcManager_->operate(deviceId,cmd);
    result.deviceId = deviceId;
    result.integrity = res.success;
    result.message = res.message;
    PLCInfo status = plcManager_->getStatus(deviceId);
    auto itDeviceStatus = std::find_if(status.deviceStatuses.begin(), status.deviceStatuses.end(), 
                                           [&deviceId](const PLCDeviceStatus& ds) {
                                               return ds.id == deviceId;
                                           });
    if(itDeviceStatus != status.deviceStatuses.end()){
        result.status = itDeviceStatus->status;
    }
    else{
        result.status = "UNKNOWN";
    }
    result.plcresult = res;
    if(cmd == "ON"){
        auto start_time = std::chrono::steady_clock::now();
        auto timeout_time = start_time + std::chrono::seconds(10); 
        SensorData initialSensor = sensorManager_->getSensorDataRealTime(sensorId);
        float initialHumidity = initialSensor.humidity;
        float targetHumidity = initialHumidity + 5.0f;
        bool isSuccess = false;
        auto check_2s = start_time + std::chrono::seconds(2);
        auto check_4s = start_time + std::chrono::seconds(4);
        auto check_8s = start_time + std::chrono::seconds(8);
        SensorData currentSensor;
        while(std::chrono::steady_clock::now() < timeout_time){
            currentSensor = sensorManager_->getSensorDataRealTime(sensorId);
            float currentHumidity = currentSensor.humidity;
            if (currentHumidity >= targetHumidity) {
                isSuccess = true;
                break; // 满足条件，立即退出循环
            }
            auto now = std::chrono::steady_clock::now();
            if (now >= check_2s && now < check_4s) {
                std::cout << "2秒检测点：当前湿度 = " << currentHumidity << "%（基准=" << initialHumidity << "%）" << std::endl;
                check_2s = timeout_time; // 避免重复打印2s检测日志
            } else if (now >= check_4s && now < check_8s) {
                std::cout << "4秒检测点：当前湿度 = " << currentHumidity << "%（基准=" << initialHumidity << "%）" << std::endl;
                check_4s = timeout_time; // 避免重复打印4s检测日志
            } else if (now >= check_8s && now < timeout_time) {
                std::cout << "8秒检测点：当前湿度 = " << currentHumidity << "%（基准=" << initialHumidity << "%）" << std::endl;
                check_8s = timeout_time; // 避免重复打印8s检测日志
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        result.isSuccess = isSuccess;
        if(result.isSuccess) return result;
        else{
            result.plcresult.integrity = false;
            result.plcresult.message = "Verify failed, please check whether it is opened";
            result.plcresult.status = "0";
            result.sensorData.data.id = sensorId;
            result.sensorData.data.status = currentSensor.status;
            result.sensorData.data.temperature = currentSensor.temperature;
            result.sensorData.data.humidity = currentSensor.humidity;
            RealImage realImage;
            CameraStaticInfo info;
            info.camera_id = cameraId;
            FrameData frame;
            
            realImage.sourceCameraId = cameraId;
            // 调用 CameraManager 获取关键帧
            bool ok = cameraManager_->getCameraLastKeyFrame(info, frame);
            if(ok){
                realImage.frame = frame;
                realImage.integrity = true;
            }
            result.camData = realImage;
            return result;
        }
    }
    return result;
}
PLCDeviceState DeviceManager::getPLCDeviceStatus(const std::string& deviceId)
{
    PLCDeviceState res;
    if(!plcManager_){
        std::cerr << "DeviceManager: plcManager is null!"<<std::endl;
        return res;
    }
    PLCInfo status = plcManager_->getStatus(deviceId);
    auto itDeviceStatus = std::find_if(status.deviceStatuses.begin(), status.deviceStatuses.end(), 
                                           [&deviceId](const PLCDeviceStatus& ds) {
                                               return ds.id == deviceId;
                                           });
    if(itDeviceStatus != status.deviceStatuses.end()){
        res.data = *itDeviceStatus;
    }
    return res;
}

CarControlResult DeviceManager::operateCarControl(const std::string& carControlId, int motor1, int motor2)
{
    CarControlResult r;
    if (!carControlManager_) { r.success = false; r.message = "no carControlManager"; return r; }
    return carControlManager_->operate(carControlId, motor1, motor2);
}



void DeviceManager::updateConfig()
{

}
RealSensorData DeviceManager::getSensorData(const std::string& sensorId)
{
    RealSensorData rsd;
  if (!sensorManager_) return rsd;

    // 优先尝试实时读取
    auto opt = sensorManager_->getSensorDataRealTime(sensorId);
    if (opt) {
        rsd.data = *opt;
        return rsd;
    }

    // 若实时读取失败，则返回缓存数据（如果有）
    auto optc = sensorManager_->getSensorDataCached(sensorId);
    if (optc) {
        rsd.data = *optc;
    }
    return rsd;
}