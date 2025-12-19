#include "task.h"
#include "mqtt_service.h"
#include "json.hpp"
#include "image_processor.h"
#include "config_info.h"
#include "config_parser.h"
#include "mqtt_topics.h"
#include "find_video_url.h"
#define BOX_ID  ConfigParser::getInstance().getConfig().boxId  
void GetCameraRealImageTask::run(TaskContext& ctx)
{
    {
        nlohmann::json ack;
        ack["success"] = true;
        ack["cameraId"] = camId_;
        ctx.publisher->publish(RESULT_GET_REAL_IMAGE_TOPIC, ack.dump());
    } 
    nlohmann::json j;
    RealImage image = ctx.devMgr->getRealImage(camId_);
    image_buffer_t out_image;
    std::vector<unsigned char> outJpeg;
    if(image.integrity)
    {
        ImageProcessor::avframeToRGB(image.frame.frame.get(),640,640,&out_image);
        ImageProcessor::compressToJpeg(&out_image,outJpeg);
        std::string imageBase64 = ImageProcessor::jpegToBase64(outJpeg);
        j["cameraId"] =  camId_;
        j["image"] = imageBase64;
        j["commandCode"] = "0000 0200";
        ctx.publisher->publish(RESULT_GET_REAL_IMAGE_TOPIC, j.dump());
    }
    else{
        j["code"] = "no image";
        j["commandCode"] = "0000 0200";
        ctx.publisher->publish(RESULT_GET_REAL_IMAGE_TOPIC, j.dump());
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

}

void OperateValveTask::run(TaskContext& ctx)
{

    {
        nlohmann::json ack;
        ack["success"] = true;
        ack["deviceId"] = deviceId_;
        ctx.publisher->publish(RESULT_OPERATE_PLC_TOPIC, ack.dump());
    }
    OperatePLC res = ctx.devMgr->operatePlc(deviceId_, cmd_);
    nlohmann::json j;
    if(!res.integrity) j["code"] = "operate failed";
    if(cmd_ == "ON") j["commandCode"] = "0000 0100";
    else if(cmd_ == "OFF") j["commandCode"] = "0000 0101";
    j["deviceId"] = deviceId_;
    j["message"] = res.message;
    j["status"] = res.status;
    ctx.publisher->publish(RESULT_OPERATE_PLC_TOPIC, j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void OperateValueWithVerifyTask::run(TaskContext& ctx)
{

    auto result = ctx.devMgr->operatePlcWithVerify(deviceId_, cmd_, sensorId_, cameraId_);
    nlohmann::json j;
    if(cmd_ == "ON") j["commandCode"] = "0000 0100";
    else if(cmd_ == "OFF") j["commandCode"] = "0000 0101";
    if(!result.isSuccess){
        //j["isSuccess"] = result.isSuccess;
        j["deviceId"] = deviceId_;
        j["message"] = result.plcresult.message;
        j["deviceStatus"] = result.plcresult.status;

        if(result.camData.integrity){
            std::vector<unsigned char> outJpeg;
            image_buffer_t out_image;
            ImageProcessor::avframeToRGB(result.camData.frame.frame.get(),640,640,&out_image);
            ImageProcessor::compressToJpeg(&out_image,outJpeg);
            std::string imageBase64 = ImageProcessor::jpegToBase64(outJpeg);

            j["cameraId"] = cameraId_;
            j["imageBase"] = imageBase64;
        }
        j["sensorId"] = sensorId_;
        j["temperature"] = result.sensorData.data.temperature;
        j["humidity"] = result.sensorData.data.humidity;
    }
    else{
        j["deviceId"] = deviceId_;
        j["message"] = result.plcresult.message;
        j["deviceStatus"] = result.plcresult.status;
    }

    ctx.publisher->publish(RESULT_OPERATE_PLC_WITH_VERIFY_TOPIC, j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

/*
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
    ctx.publisher->publish(RESULT_GET_SENSOR_DATA_TOPIC, j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
*/
void GetDeviceStatusTask::run(TaskContext& ctx)
{
    
    {
        nlohmann::json ack;
        ack["success"] = true;  // 由于是获取所有设备的，所以不需要返回设备信息
        ctx.publisher->publish(RESULT_GET_ALL_DEVICE_STATUS_TOPIC, ack.dump());
    }
    DeviceStatus status = ctx.devMgr->getStatus();

    nlohmann::json j;
    j["commandCode"] = "0000 0001";
    j["deviceId"]=BOX_ID;
    auto& device = j["device"];
    
    for(const auto& camStatus : status.cameraStatusList.cameraStatus){
        device["cameras"].push_back({
            {"cameraId",camStatus.camera_id},
            {"onlineStatus",camStatus.online_status == CameraOnlineStatus::ONLINE ? "ONLINE" : "OFFLINE"}
        });
    }

    for(const auto& devStatus : status.plcStatus_.plcList){
        for(const auto& dev : devStatus.deviceStatuses){
            device["plc_device"].push_back({
                {"deviceId", dev.id},
                {"name", dev.name},
                {"status", dev.status}
            });
        }
    }

    for (const auto& sensor : status.sensorStatus_.sensors) {
        nlohmann::json s;
        s["id"] = sensor.id;
        s["type"] = sensor.type;
        s["status"] = to_string(sensor.status); 

        if (sensor.status == SensorStatus::NORMAL) {
            if (sensor.type == "modbus") {
                s["temperature"] = sensor.temperature;
                s["humidity"] = sensor.humidity;
            } else if (sensor.type == "gpio" || sensor.type == "custom") {
                s["value"] = sensor.value;
            }
        } else {
            s["code"] = "no data";
        }
        device["sensor"].push_back(s);
    }

    ctx.publisher->publish(RESULT_GET_ALL_DEVICE_STATUS_TOPIC, j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}


void CarControlTask::run(TaskContext& ctx)
{
    
    {
        nlohmann::json ack;
        ack["success"] = true;  // 没提供小车的信息
        ack["carcontrolId"] = payload_["carcontrolId"];
        ctx.publisher->publish(RESULT_OPERATE_CAR_TOPIC, ack.dump());
    }
    if (!validatePayload(payload_)) {
        nlohmann::json errorResult;
        errorResult["success"] = false;
        errorResult["error"] = "Invalid payload parameters";
        ctx.publisher->publish(RESULT_OPERATE_CAR_TOPIC, errorResult.dump());
        return;
    }
    
    std::string carId = payload_.value("carcontrolId", std::string("carcontrol001"));
    int motor1 = payload_.value("motor1", 0);
    int motor2 = payload_.value("motor2", 0);
    
    motor1 = std::max(-1500, std::min(1500, motor1));
    motor2 = std::max(-1500, std::min(1500, motor2));
    
    std::cout << "Executing car control: car_id=" << carId 
              << ", motor1=" << motor1 << ", motor2=" << motor2 << std::endl;
    
    CarControlResult result = ctx.devMgr->operateCarControl(carId, motor1, motor2);
    
    nlohmann::json jsonResponse;
    jsonResponse["success"] = result.success;
    jsonResponse["carcontrolId"] = carId;
    jsonResponse["motor1"] = result.motor1;
    jsonResponse["motor2"] = result.motor2;
    
    //ztl
    // 状态映射逻辑
    int status = 0;
    uint16_t statusByte = result.statusByte;
    if (statusByte == 514) { // 0x0202
        status = 0; // 正常
    } else if (statusByte == 0) {
        status = -1; // 无响应(电机停转)
    } else if (statusByte == 0x0303) { // 00000011 00000011
        status = 1; // 电机堵转
    } else if (statusByte == 0x0C0C) { // 00000012 00000012  (十六进制 0x0C = 十进制 12)
        status = 2; // 电流保护
    }
    
    jsonResponse["status"] = status;
    //ztl
    
    // jsonResponse["status_byte"] = result.statusByte;
    // jsonResponse["message"] = result.message;
    
    ctx.publisher->publish(RESULT_OPERATE_CAR_TOPIC, jsonResponse.dump());
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

bool CarControlTask::validatePayload(const nlohmann::json& payload)
{
    if (!payload.contains("motor1") && !payload.contains("motor2")) {
        return false;
    }
    
    if (payload.contains("motor1") && !payload["motor1"].is_number_integer()) {
        return false;
    }
    
    if (payload.contains("motor2") && !payload["motor2"].is_number_integer()) {
        return false;
    }

    
    return true;
}

void UpdateConfigTask::run(TaskContext& ctx){
    

    {
        nlohmann::json ack;
        ack["success"] = true;
        ctx.publisher->publish(RESULT_UPDATE_CONFIG, ack.dump());
    }
    UpdateConfigResult res = ctx.devMgr->configUpdate(JsonStr_);
    nlohmann::json j;
    j["commandCode"] = "0000 0050";
    j["deviceId"] = BOX_ID;
    if(!res.isSuccess) j["code"] = "update failed";
    j["message"] = res.message;
    ctx.publisher->publish(RESULT_UPDATE_CONFIG, j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void DownloadVideoTask::run(TaskContext& ctx){
    {
        nlohmann::json ack;
        ack["success"] = true;
        ctx.publisher->publish(RESULT_DOWNLOAD_VIDEO, ack.dump());
    }
    nlohmann::json j;
    std::string videoPath = findVideoUrl(channel_, date_, time_);
    if(videoPath.empty()){
        j["success"] = false;
        j["message"] = "video file not found";
        ctx.publisher->publish(RESULT_DOWNLOAD_VIDEO, j.dump());
        return;
    }
    j["success"] = true;
    j["videoPath"] = videoPath;
    ctx.publisher->publish(RESULT_DOWNLOAD_VIDEO, j.dump());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}