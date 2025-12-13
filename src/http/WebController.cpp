#include "WebController.h"
#include <iostream>
#include "image_processor.h"
#include "task.h"
#include "task_result_publisher.h"

WebController::WebController(const std::string& /*configPath*/, IDeviceManager* devMgr, JobScheduler* scheduler)
    : devMgr_(devMgr), scheduler_(scheduler)
{
}

json WebController::handleJson(const json& payload)
{
    json resp;
    resp["success"] = false;
    resp["error"] = "use handleHttp for routing";
    return resp;
}

json WebController::handleHttp(const std::string& path, const json& payload)
{
    json resp;

    if (!scheduler_) {
        resp["success"] = false;
        resp["error"] = "scheduler not available";
        return resp;
    }

    // -------------------------------
    // 1) Camera getRealImage  → 提交任务
    // -------------------------------
    if (path.find("/device/camera") != std::string::npos) {
        std::string camId = payload.value("cameraId", payload.value("deviceId", ""));
        if (camId.empty()) {
            resp["success"] = false;
            resp["error"] = "no camera id";
            return resp;
        }

        auto task = std::make_shared<GetCameraRealImageTask>(camId);
        int id = scheduler_->submit(task, "http");

        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }

    // -------------------------------
    // 2) PLC operate → 提交任务
    // -------------------------------
    if (path.find("/device/plc/operate") != std::string::npos ||
        path.find("/device/control/solenoid") != std::string::npos) {

        std::string deviceId = payload.value("deviceId", payload.value("plc_device_id", ""));
        std::string action = payload.value("action", "");

        if (deviceId.empty()) {
            resp["success"] = false;
            resp["error"] = "no device id";
            return resp;
        }

        auto task = std::make_shared<OperateValveTask>(deviceId, action);
        int id = scheduler_->submit(task, "http");

        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }

    // -------------------------------
    // 3) 车控制 → 提交任务
    // -------------------------------
    if (path.find("/device/carControl") != std::string::npos) {

        auto task = std::make_shared<CarControlTask>(payload);
        int id = scheduler_->submit(task, "http");

        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }

    // -------------------------------
    // 4) get sensor → 提交任务
    // -------------------------------
    // if (path.find("/device/sensor/get") != std::string::npos) {

    //     if (!payload.contains("sensorId")) {
    //         resp["success"] = false;
    //         resp["error"] = "missing sensorId";
    //         return resp;
    //     }

    //     auto task = std::make_shared<GetSensorDataTask>(payload["sensorId"].get<std::string>());
    //     int id = scheduler_->submit(task, "http");

    //     resp["success"] = true;
    //     resp["task_id"] = id;
    //     return resp;
    // }

    // 获取全部状态
    if(path.find("/device/getAll") != std::string::npos) {
        auto task = std::make_shared<GetDeviceStatusTask>();
        int id = scheduler_->submit(task, "http");
        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }

    // -------------------------------
    // 5) 其它未知路径，统一接受但不执行任务
    // -------------------------------
    resp["success"] = true;
    resp["note"] = "accepted";
    return resp;
}
