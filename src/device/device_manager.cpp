#include "device_manager.h"
#include "camera_manager.h"
#include "sensor_manager.h"
#include "plc_manager.h"
#include <iostream>
DeviceManager::DeviceManager()
{
    cameraManager_ = std::make_shared<CameraManager>();
    sensorManager_ = std::make_shared<SensorManager>();
    plcManager_ = std::make_shared<PLCManager>();
}

DeviceManager::~DeviceManager()
{}

DeviceStatus DeviceManager::getStatus()
{
    DeviceStatus deviceStatus;
    deviceStatus.cameraStatus_ = cameraManager_->getAllStatus();
    if(plcManager_){
        std::vector<PLCInfo> allPLCInfo = plcManager_->getAllStatus();
        for(const auto& plcInfo : allPLCInfo){
            for(const auto& devStatus : plcInfo.deviceStatuses){
                PLCDeviceState plcDevState;
                plcDevState.data = devStatus;
                deviceStatus.plcStatus_.push_back(plcDevState);
            }
        }
    }
    
    if(sensorManager_{
        deviceStatus.sensorStatus_ = sensorManager_ -> getAllSensorData();
    })

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