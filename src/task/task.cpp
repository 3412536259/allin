#include "task.h"
#include "mqtt_service.h"
#include "json.hpp"

void GetCameraRealImageTask::run(TaskContext& ctx)
{
    ctx.devMgr->getRealImage(camId_);
    nlohmann::json j;
    j["cameraId"] =  camId_;
    ctx.mqtt->publish("device/camera/getRealImage", j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
