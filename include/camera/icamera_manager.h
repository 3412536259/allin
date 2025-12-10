#ifndef I_CAMERA_MANAGER_H
#define I_CAMERA_MANAGER_H
#include "camera_info.h"
#include <vector>
#include <map>
class ICameraManager{
public:
    virtual ~ICameraManager() = default;
    // virtual void start() = 0;
    // virtual void stop() = 0;
    virtual CameraStatus  getStatus(const CameraStaticInfo& info) = 0; //获取单个摄像头状态
    virtual CameraStatusList  getAllStatus() = 0; //获取所有摄像头状态
    virtual bool getCameraLastKeyFrame(const CameraStaticInfo& info, FrameData& out) = 0; //获取缓存中的帧（单个摄像头）
    virtual std::map<std::string, FrameData> getAllLastKeyFrames() = 0;

};


#endif