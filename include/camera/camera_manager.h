#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H
#include "icamera_manager.h"
#include "camera.h"
#include <mutex>
#include <atomic>

class CameraManager :public ICameraManager{
public:    
    CameraManager();
    ~CameraManager();

    void start() override;
    void stop() override;

    CameraStatus  getStatus(int id) override;
    std::map<int, CameraStatus>  getAllStatus() override;
    bool getCameraLastKeyFrame(int id, FrameData& out) override;
private:
    bool addCamera(int id, const CameraStaticInfo& info);
    bool removeCamera(int id);
    bool registerDevices();
private:
    std::map<int, std::unique_ptr<Camera>> cameras_;
    std::mutex mutex_;
    std::atomic_bool running_ = false;
};






#endif

