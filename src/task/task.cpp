#include "task.h"
#include "mqtt_service.h"
#include "json.hpp"
#include "image_processor.h"
#include "config_info.h"
#include "config_parser.h"
void GetCameraRealImageTask::run(TaskContext& ctx)
{
    RealImage image = ctx.devMgr->getRealImage(camId_);
    image_buffer_t out_image;
    std::vector<unsigned char> outJpeg;
    if(image.integrity)
    {
        ImageProcessor::avframeToRGB(image.frame.frame.get(),640,640,&out_image);
        ImageProcessor::compressToJpeg(&out_image,outJpeg);
        std::string imageBase64 = ImageProcessor::jpegToBase64(outJpeg);
        nlohmann::json j;
        j["cameraId"] =  camId_;
        j["image"] = imageBase64;
        ctx.publisher->publish("device/camera/result/getRealImage", j.dump());
    }
    else{
        nlohmann::json j;
        j["code"] = "no image";
        ctx.publisher->publish("device/camera/result/getRealImage", j.dump());
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

}

void OperateValveTask::run(TaskContext& ctx)
{
    OperatePLC res = ctx.devMgr->operatePlc(deviceId_, cmd_);
    nlohmann::json j;
    if(!res.integrity) j["code"] = "operate failed";
    j["deviceId"] = deviceId_;
    j["message"] = res.message;
    j["status"] = res.status;
    DeviceConfigRoot cfg = ConfigParser::getInstance().getConfig();
    std::string theme = "box" + cfg.boxId + "device/control/solenoid/direct/result";
    ctx.publisher->publish(theme, j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}