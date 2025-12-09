#pragma once

#include <string>
#include "json.hpp"

using nlohmann::json;



enum class CommandType {
    Unknown,
    SolenoidDirect,
    SolenoidVerified,
    SensorCustom,
    SensorGPIO,
    SensorModbus,
    PumpDirect,
    Camera
};

struct Command {
    CommandType type = CommandType::Unknown;
    std::string boxId;
    std::string plc_device_id;
    std::string plc_id;
    std::string action;
    json raw; 

    static bool fromJson(const json& j, const std::string& boxId, Command& out, std::string& err)
    {
        out = Command();
        out.raw = j;
        out.boxId = boxId;

        if (j.contains("plc_device_id") && j.contains("action")) {
            out.plc_device_id = j.value("plc_device_id", "");
            out.plc_id = j.value("plc_id", "");
            out.action = j.value("action", "");
            out.type = CommandType::SolenoidDirect;
            return true;
        }

        if (j.contains("device_id") && j.contains("verification")) {
            out.plc_device_id = j.value("device_id", "");
            out.plc_id = j.value("plc_id", "");
            out.action = j.value("action", "");
            out.type = CommandType::SolenoidVerified;
            return true;
        }

        if (j.contains("sensor_id") && j.contains("value")) {
            out.type = CommandType::SensorCustom;
            return true;
        }

        if (j.contains("sensor_id") && j.contains("status") && j.contains("value")) {
            out.type = CommandType::SensorGPIO;
            return true;
        }

        if (j.contains("sensor_id") && (j.contains("temperature") || j.contains("humidity"))) {
            out.type = CommandType::SensorModbus;
            return true;
        }

        if (j.contains("carmeId") || j.contains("cameraId") || j.contains("camera_id")) {
            out.type = CommandType::Camera;
            return true;
        }

        err = "unknown command format";
        return false;
    }
};


