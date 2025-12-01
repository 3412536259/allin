#include "camera_manager.h"
#include <iostream>
CameraManager::CameraManager(){}

CameraManager::~CameraManager()
{
    stop();
}


bool CameraManager::registerDevices()
{
    // TODO: 根据你自己的业务从 DB / 配置文件 / 网络拉取列表

    // 示例：加载两个摄像头
    CameraStaticInfo info1;
    info1.camera_id = 1;
    info1.rtsp_url = "rtsp://admin:Wlkjaqxy411@10.9.255.21:554/Streaming/Channels/101";

    CameraStaticInfo info2;
    info2.camera_id = 2;
    info2.rtsp_url = "rtsp://admin:Wlkjaqxy411@10.9.255.21:554/Streaming/Channels/201";

    addCamera(1, info1);
    addCamera(2, info2);

    return true;
}


bool CameraManager::addCamera(int id, const CameraStaticInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (cameras_.count(id))
        return false;

    cameras_[id] = std::make_unique<Camera>(info);
    return true;
}

bool CameraManager::removeCamera(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!cameras_.count(id))
        return false;

    cameras_[id]->stop();
    cameras_.erase(id);
    return true;
}


void CameraManager::start()
{  
    if(running_)
    {
        return;
    }

    running_ = true;
    if(!registerDevices())
    {   
        std::cout << "camera devices regist failed." << std::endl;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto& camera : cameras_)
    {
        camera.second->start();
    }
}

void CameraManager::stop()
{
    if (!running_) 
    {
        return;
    }    
    running_ = false;

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& camera : cameras_) {
        camera.second->stop();
    }
}


CameraStatus CameraManager::getStatus(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!cameras_.count(id)) {
        CameraStatus st;
        st.online_status = CameraOnlineStatus::OFFLINE;
        return st;
    }

    return cameras_[id]->getStatus();
}

std::map<int, CameraStatus> CameraManager::getAllStatus() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::map<int, CameraStatus> result;
    for (auto& camera : cameras_) {
        result[camera.first] = camera.second->getStatus();
    }
    return result;
}

bool CameraManager::getCameraLastKeyFrame(int id, FrameData& out) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!cameras_.count(id))
        return false;

    return cameras_[id]->getLastKeyFrame(out);
}