#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H
#include "icamera_manager.h"

class CameraManager :public ICameraManager{
public:    
    CameraManager();
    ~CameraManager();

    void start() override;
    void stop() override;

    CameraStatus  getState(int id) override;
    CameraStatus  getStates() override;


};






#endif

