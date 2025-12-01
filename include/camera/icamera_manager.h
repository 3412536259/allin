#ifndef I_CAMERA_MANAGER_H
#define I_CAMERA_MANAGER_H
#include "camera_info.h"
#include <map>
class ICameraManager{
public:
    virtual ~ICameraManager() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual CameraStatus  getStatus(int id) = 0; //获取单个摄像头状态
    virtual std::map<int, CameraStatus>  getAllStatus() = 0; //获取所有摄像头状态
    virtual bool getCameraLastKeyFrame(int id, FrameData& out) = 0; //获取缓存中的帧（单个摄像头）


};






#endif