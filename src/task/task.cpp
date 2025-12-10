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

void GetPLCDeviceTask::run(TaskContext& ctx)
{
    PLCDeviceState status = ctx.devMgr->getPLCDeviceStatus(deviceId_);
    nlohmann::json j;
    j["deviceId"] = status.data.id;
    j["name"] = status.data.name;
    j["status"] = status.data.status;
    ctx.publisher->publish("device/control/result", j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}


void GetSensorDataTask::run(TaskContext& ctx)
{
    RealSensorData rsd = ctx.devMgr->getSensorData(sensorId_);
    const SensorData& data = rsd.data;
    nlohmann::json j;
    j["sensorId"] = sensorId_;
    j["type"] = data.type;
    j["status"] = to_string(data.status);

    if(data.status == SensorStatus::NORMAL)
    {
        if (data.type == "modbus") {
            j["temperature"] = data.temperature;
            j["humidity"] = data.humidity;
        }
        if (data.type == "gpio" || data.type == "custom") {
            j["value"] = data.value;
        }
    }
    else
    {
        j["code"] = "no data";
    }
    ctx.publisher->publish("device/sensor/result/getSensorData", j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}