#include "CommandTask.h"
#include "json.hpp"
#include "image_processor.h"
#include "mqtt_service.h"
#include <iostream>

using nlohmann::json;

void CommandTask::run(TaskContext& ctx)
{
    try {
        switch (cmd_.type) {
            case CommandType::Camera: {
                RealImage image = ctx.devMgr->getRealImage(cmd_.raw.value("carmeId", cmd_.raw.value("cameraId", std::string(""))));
                if (image.integrity) {
                    image_buffer_t out_image;
                    std::vector<unsigned char> outJpeg;
                    ImageProcessor::avframeToRGB(image.frame.frame.get(),640,640,&out_image);
                    ImageProcessor::compressToJpeg(&out_image,outJpeg);
                    std::string imageBase64 = ImageProcessor::jpegToBase64(outJpeg);
                    json j;
                    j["cameraId"] = cmd_.raw.value("carmeId", cmd_.raw.value("cameraId", std::string("")));
                    j["image"] = imageBase64;
                    if (ctx.mqtt) ctx.mqtt->publish("box/" + cmd_.boxId + "/device/camera/result/getRealImage", j.dump());
                } else {
                    json j; j["code"] = "no image";
                    if (ctx.mqtt) ctx.mqtt->publish("box/" + cmd_.boxId + "/device/camera/result/getRealImage", j.dump());
                }
                break;
            }
            case CommandType::SolenoidDirect:
            case CommandType::PumpDirect: {
                if (!cmd_.plc_device_id.empty()) {
                    ctx.devMgr->operatePlc(cmd_.plc_device_id, cmd_.action);
                    json j;
                    j["plc_device_id"] = cmd_.plc_device_id;
                    j["action"] = cmd_.action;
                    j["result"] = "sent";
                    if (ctx.mqtt) ctx.mqtt->publish("box/" + cmd_.boxId + "/device/plc/result/operate", j.dump());
                }
                break;
            }
            case CommandType::SolenoidVerified: {
                bool ok = true;
                if (cmd_.raw.contains("verification")) {
                    for (auto &v : cmd_.raw["verification"]) {
                        std::string type = v.value("type", "");
                        if (type == "image") {
                            std::string cam = v.value("camera_id", "");
                            RealImage image = ctx.devMgr->getRealImage(cam);
                            if (!image.integrity) { ok = false; break; }
                        } else if (type == "temperature") {
                            ok = false;
                            break;
                        }
                    }
                }
                if (ok) {
                    ctx.devMgr->operatePlc(cmd_.plc_device_id, cmd_.action);
                }
                json j;
                j["plc_device_id"] = cmd_.plc_device_id;
                j["action"] = cmd_.action;
                j["verified"] = ok;
                if (ctx.mqtt) ctx.mqtt->publish("box/" + cmd_.boxId + "/device/plc/result/operate_verified", j.dump());
                break;
            }
            case CommandType::SensorCustom:
            case CommandType::SensorGPIO:
            case CommandType::SensorModbus: {
                if (ctx.mqtt) {
                    json j = cmd_.raw;
                    j["box_id"] = cmd_.boxId;
                    ctx.mqtt->publish("box/" + cmd_.boxId + "/device/sensor/report", j.dump());
                }
                break;
            }
            default:
                std::cout << "CommandTask: unknown command type" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "CommandTask exception: " << e.what() << std::endl;
    }
}