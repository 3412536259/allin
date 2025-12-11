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
    // This method is now deprecated for HTTP routing use; keep basic echo behavior
    json resp;
    resp["success"] = false;
    resp["error"] = "use handleHttp for routing";
    return resp;
}

json WebController::handleHttp(const std::string& path, const json& payload)
{
    json resp;

    // 1) Camera synchronous image fetch
    if (path.find("/device/camera") != std::string::npos) {
        std::string camId;
        if (payload.contains("cameraId")) camId = payload["cameraId"].get<std::string>();
        if (payload.contains("deviceId")) camId = payload["deviceId"].get<std::string>();
        if (camId.empty()) {
            resp["success"] = false; resp["error"] = "no camera id"; return resp;
        }
        GetCameraRealImageTask t(camId);
        HttpPublisher hp;
        TaskContext ctx{.taskId = 0, .devMgr = devMgr_, .publisher = &hp, .source = std::string("http")};
        t.run(ctx);
        std::string msg = hp.waitForMessage(1000);
        if (msg.empty()) { resp["success"] = false; resp["error"] = "no result"; return resp; }
        try { return json::parse(msg); } catch(...) { resp["result"] = msg; return resp; }
    }

    // 2) PLC operate (solenoid/pump) synchronous
    if (path.find("/device/plc/operate") != std::string::npos || path.find("/device/control/solenoid") != std::string::npos) {
        if(!payload.contains("deviceId") && !payload.contains("plc_device_id")) {
            resp["success"] = false; resp["error"] = "no device id"; return resp;
        }
        std::string deviceId = payload.contains("deviceId") ? payload["deviceId"].get<std::string>() : payload.value("plc_device_id", std::string(""));
        std::string action = payload.value("action", "");
        OperateValveTask t(deviceId, action);
        HttpPublisher hp;
        TaskContext ctx{.taskId = 0, .devMgr = devMgr_, .publisher = &hp, .source = std::string("http")};
        t.run(ctx);
        std::string msg = hp.waitForMessage(1000);
        if (msg.empty()) { resp["success"] = false; resp["error"] = "no result"; return resp; }
        try { return json::parse(msg); } catch(...) { resp["result"] = msg; return resp; }
    }

    // 3) Car control synchronous
    if (path.find("/device/control/carcontrol") != std::string::npos) {
        CarControlTask t(payload);
        HttpPublisher hp;
        TaskContext ctx{.taskId = 0, .devMgr = devMgr_, .publisher = &hp, .source = std::string("http")};
        t.run(ctx);
        std::string msg = hp.waitForMessage(1000);
        if (msg.empty()) { resp["success"] = false; resp["error"] = "no result"; return resp; }
        try { return json::parse(msg); } catch(...) { resp["result"] = msg; return resp; }
    }

    // 4) Sensor endpoints: accept report payloads and publish via scheduler as sensor-report tasks or immediate ack
    if (path.find("/device/sensor") != std::string::npos) {
        // publish report immediately using scheduler's publisher if available via submitting a small task
        // create a minimal lambda-task by reusing GetSensorDataTask when requesting data, otherwise just ack
        if (payload.contains("sensor_id")) {
            // ack and let system handle incoming sensor data (could be extended)
            resp["success"] = true; resp["received"] = payload;
            return resp;
        }
    }

    // default: submit as async task via scheduler with source=http if scheduler available
    if (scheduler_) {
        // attempt to map to some known tasks: sensor get
        if (path.find("/device/sensor/get") != std::string::npos && payload.contains("sensorId")) {
            auto st = std::make_shared<GetSensorDataTask>(payload["sensorId"].get<std::string>());
            int id = scheduler_->submit(st, "http");
            resp["success"] = true; resp["task_id"] = id; return resp;
        }
        // fallback: ack
        resp["success"] = true; resp["note"] = "accepted"; return resp;
    }

    resp["success"] = false; resp["error"] = "no handler";
    return resp;
}
