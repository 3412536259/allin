#ifndef CAMERA_H
#define CAMERA_H
#include "camera_info.h"
#include "video_capture.h"
#include <atomic>
#include <thread>
#include <mutex>
class Camera{
public:
    Camera(CameraStaticInfo cameraStaticInfo);
    ~Camera();
    bool start();
    bool stop();
    CameraStatus getStatus();
    bool isRunning() const;
    bool getLastKeyFrame(FrameData& out);
private:
    void pullKeyFrameLoop();

private:
    
    CameraStatus cameraStatus_;//自身的状态,动态变化
    CameraStaticInfo cameraStaticInfo_;//静态状态，访问少
    
    FrameData lastKeyFrame_; //关键帧数据缓存
    std::thread pullThread_; //拉流线程，先自己管理线程
    std::atomic_bool isRunning_ = false; //该摄像头是否运行（拉流存关键帧到缓存）
    VideoCapture videoCapture_;    
    std::mutex keyFrameMutex_;
    std::mutex statusMutex_;
    
};



#endif