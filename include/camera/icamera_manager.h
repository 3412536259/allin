#ifndef I_CAMERA_MANAGER_H
#define I_CAMERA_MANAGER_H
#include <string>
struct CameraStaticInfo {
    int camera_id;
    std::string name;
    std::string rtsp_url;
    bool ptz_supported;
};

//摄像头实时状态，云端查询状态，盒子上报状态时使用
struct CameraStatus {
    bool online;
    std::string status;  // "IDLE", "STREAMING", "ERROR"
    int error_code;
    // long long last_heartbeat_time;

    // // 流状态
    // bool is_streaming;
    // int current_frame_rate;

    // // PTZ 当前值
    // int pan;
    // int tilt;
    // int zoom;
};


class ICameraManager{
public:
    virtual ~ICameraManager() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual CameraStatus  getState(int id) = 0;
    virtual CameraStatus  getStates() = 0;
};






#endif