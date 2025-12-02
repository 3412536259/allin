#include "camera_manager.h"
#include <iostream>
CameraManager::CameraManager()
{
    start();
}

CameraManager::~CameraManager()
{
    stop();
}


bool CameraManager::registerDevices()
{
    // TODO: 根据你自己的业务从 DB / 配置文件 / 网络拉取列表

    // 示例：加载两个摄像头
    CameraStaticInfo info1;
    info1.camera_id = "1";
    info1.rtsp_url = "rtsp://admin:Wlkjaqxy411@10.9.255.21:554/Streaming/Channels/101";

    CameraStaticInfo info2;
    info2.camera_id = "2";
    info2.rtsp_url = "rtsp://admin:Wlkjaqxy411@10.9.255.21:554/Streaming/Channels/201";

    addCamera(info1);
    addCamera(info2);

    return true;
}


bool CameraManager::addCamera(const CameraStaticInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = info.camera_id;
    if (cameras_.count(id))
        return false;

    cameras_[id] = std::make_unique<Camera>(info);
    return true;
}

bool CameraManager::removeCamera(const CameraStaticInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = info.camera_id;
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


CameraStatus CameraManager::getStatus(const CameraStaticInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = info.camera_id;
    if (!cameras_.count(id)) {
        CameraStatus st;
        st.online_status = CameraOnlineStatus::OFFLINE;
        return st;
    }

    return cameras_[id]->getStatus();
}

std::vector<CameraStatus> CameraManager::getAllStatus() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<CameraStatus> result;
    for (auto& camera : cameras_) {
        result.push_back(camera.second->getStatus());
    }
    return result;
}

bool CameraManager::getCameraLastKeyFrame(const CameraStaticInfo& info, FrameData& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = info.camera_id;
    if (!cameras_.count(id))
        return false;

    return cameras_[id]->getLastKeyFrame(out);
}

std::map<std::string, FrameData> CameraManager::getAllLastKeyFrames()
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::map<std::string, FrameData> result;

    for (auto& kv : cameras_) {
        std::string id = kv.first;
        FrameData frame;

        if (kv.second->getLastKeyFrame(frame)) {
            result[id] = frame;  // 成功获取则加入 map
        } else {
            // 获取失败时可选择加入空帧或跳过
            std::cerr << "CameraManager: Failed to get keyframe for camera "
                      << id << std::endl;
        }
    }

    return result;
}
