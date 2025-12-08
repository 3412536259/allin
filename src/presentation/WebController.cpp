#include "WebController.h"
#include "ConfigUtil.h"
#include <iostream>
#include "CommandTask.h"
#include "image_processor.h"


WebController::WebController(const std::string& configPath, IDeviceManager* devMgr, JobScheduler* scheduler)
    : devMgr_(devMgr), scheduler_(scheduler)
{
    if (!ConfigUtil::loadBoxId(configPath, boxId_)) {
        std::cerr << "WebController: failed to load box id from " << configPath << "\n";
        boxId_.clear();
    }
}

json WebController::handleJson(const json& payload)
{
    json resp;
    std::string err;
    Command cmd;
    if (!Command::fromJson(payload, boxId_, cmd, err)) {
        resp["success"] = false;
        resp["error"] = err;
        return resp;
    }

    if (cmd.type == CommandType::Camera && devMgr_ != nullptr) {
        std::string camId = cmd.raw.value("carmeId", cmd.raw.value("cameraId", std::string("")));
        RealImage image = devMgr_->getRealImage(camId);
        if (image.integrity) {
            image_buffer_t out_image;
            std::vector<unsigned char> outJpeg;
            ImageProcessor::avframeToRGB(image.frame.frame.get(),640,640,&out_image);
            ImageProcessor::compressToJpeg(&out_image,outJpeg);
            std::string imageBase64 = ImageProcessor::jpegToBase64(outJpeg);
            resp["success"] = true;
            resp["cameraId"] = camId;
            resp["image"] = imageBase64;
            return resp;
        } else {
            resp["success"] = false;
            resp["error"] = "no image";
            return resp;
        }
    }

    if (scheduler_ != nullptr) {
        auto task = std::make_shared<CommandTask>(cmd);
        int id = scheduler_->submit(task);
        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }

    std::cout << "WebController: received command for box=" << cmd.boxId << " type=" << (int)cmd.type << "\n";
    resp["success"] = true;
    resp["box_id"] = cmd.boxId;
    resp["type"] = (int)cmd.type;
    resp["raw"] = cmd.raw;
    return resp;
}
