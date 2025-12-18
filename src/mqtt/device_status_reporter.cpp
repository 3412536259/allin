#include "device_status_reporter.h"
#include <chrono>
#include <iostream>
#include <json.hpp>
#include "config_parser.h"
#define BOX_ID  ConfigParser::getInstance().getConfig().boxId  
DeviceStatusReporter::DeviceStatusReporter(IDeviceManager* devMgr,
                                           ITaskResultPublisher* publisher)
    : devMgr_(devMgr), publisher_(publisher) {}

DeviceStatusReporter::~DeviceStatusReporter() {
    stopAutoReport();
}

void DeviceStatusReporter::startAutoReport(const std::string& topic, int intervalSec) {
    if (running_) return;

    topic_ = topic;
    intervalSec_ = intervalSec;
    running_ = true;

    worker_ = std::thread(&DeviceStatusReporter::autoReportLoop, this);
}

void DeviceStatusReporter::stopAutoReport() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void DeviceStatusReporter::autoReportLoop() {
    while (running_) {
        reportStatus(topic_);
        std::this_thread::sleep_for(std::chrono::seconds(intervalSec_));
    }
}

void DeviceStatusReporter::reportStatus(const std::string& topic)
{
    DeviceStatus status = devMgr_->getStatus();
    nlohmann::json j;
    j["commandCode"] = "0000 0001";
    j["deviceId"]=BOX_ID;
    auto& device = j["device"];

    // Cameras
    for (const auto& cs : status.cameraStatusList.cameraStatus) {
        device["cameras"].push_back({
            {"cameraId", cs.camera_id},
            {"onlineStatus", cs.online_status == CameraOnlineStatus::ONLINE ? "ONLINE" : "OFFLINE"}
        });
    }

    // PLC
    for (const auto& plc : status.plcStatus_.plcList) {
        for (const auto& d : plc.deviceStatuses) {
            device["plc_device"].push_back({
                {"deviceId", d.id},
                {"name",    d.name},
                {"status",  d.status}
            });
        }
    }

    // Sensors
    for (const auto& sensor : status.sensorStatus_.sensors) {
        nlohmann::json s;
        s["id"] = sensor.id;
        s["type"] = sensor.type;
        s["status"] = to_string(sensor.status);

        if (sensor.status == SensorStatus::NORMAL) {
            if (sensor.type == "modbus") {
                s["temperature"] = sensor.temperature;
                s["humidity"] = sensor.humidity;
            } else {
                s["value"] = sensor.value;
            }
        } else {
            s["code"] = "no data";
        }

        device["sensor"].push_back(s);
    }

    publisher_->publish(topic, j.dump());
}
