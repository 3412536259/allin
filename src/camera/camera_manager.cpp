#include "camera_manager.h"
#include "config_parser.h"
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
    auto& config = ConfigParser::getInstance().getConfig();
    
    for(auto& kv : config.cameras)
    {
        CameraStaticInfo info;
        info.camera_id = kv.id;
        info.rtsp_url = kv.url;
        info.name = kv.name;
        addCamera(info);
    }

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

CameraStatusList CameraManager::getAllStatus() {
    CameraStatusList cameraStatusList;
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& camera : cameras_) {
        cameraStatusList.cameraStatus.push_back(camera.second->getStatus());
    }
    return cameraStatusList;
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
