#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H
#include "icamera_manager.h"
#include "camera.h"
#include <mutex>
#include <atomic>
#include <vector>
class CameraManager :public ICameraManager{
public:    
    CameraManager();
    ~CameraManager();

    void start() override;
    void stop() override;

    CameraStatus  getStatus(const CameraStaticInfo& info) override;
    std::vector<CameraStatus>  getAllStatus() override;
    bool getCameraLastKeyFrame(const CameraStaticInfo& info, FrameData& out) override;
    std::map<std::string, FrameData> getAllLastKeyFrames() override;

private:
    bool addCamera(const CameraStaticInfo& info);
    bool removeCamera(const CameraStaticInfo& info);
    bool registerDevices();
private:
    std::map<std::string, std::unique_ptr<Camera>> cameras_;
    std::mutex mutex_;
    std::atomic_bool running_ = false;
};






#endif

