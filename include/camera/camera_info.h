#ifndef CAMERA_INFO_H
#define CAMERA_INFO_H
#include<string>
#include<memory>
#include<fstream>
#include<vector>
extern "C" {
#include <libavutil/frame.h>
}
struct CameraStaticInfo {
    std::string camera_id;
    std::string name;
    std::string rtsp_url;
    // bool ptz_supported;
};

enum class CameraOnlineStatus
{
    OFFLINE,
    ONLINE
};
// 2. 重载 << 运算符（关键：告诉编译器如何输出 CameraOnlineStatus）
inline std::ostream& operator<<(std::ostream& os, const CameraOnlineStatus& status) {
    // 根据枚举值，输出对应的字符串（语义更清晰，推荐）
    switch (status) {
        case CameraOnlineStatus::OFFLINE:
            os << "OFFLINE";  // 输出字符串"OFFLINE"
            break;
        case CameraOnlineStatus::ONLINE:
            os << "ONLINE";   // 输出字符串"ONLINE"
            break;
        default:
            os << "UNKNOWN";  // 应对意外的枚举值（避免乱码）
            break;
    }
    return os;  // 返回ostream对象，支持链式输出（如 cout << a << b）
}

struct FrameData{
    std::shared_ptr<AVFrame> frame;
    int64_t timestamp = 0;
};

//摄像头实时状态，云端查询状态，盒子上报状态时使用
struct CameraStatus {
    std::string camera_id;
    CameraOnlineStatus online_status;
    // std::string status;  // "IDLE", "STREAMING", "ERROR"
    // int error_code;
    // long long last_heartbeat_time;

    // // 流状态
    // bool is_streaming;
    // int current_frame_rate;

    // // PTZ 当前值
    // int pan;
    // int tilt;
    // int zoom;
};

struct CameraStatusList{
    std::vector<CameraStatus> cameraStatus;
};

#endif