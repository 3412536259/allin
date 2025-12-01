#include "config_parser.h"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

bool ConfigParser::loadFromFile(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "[ConfigParser] Failed to open file: " << path << "\n";
        return false;
    }

    json j;
    try {
        ifs >> j;
    } catch (std::exception& e) {
        std::cerr << "[ConfigParser] JSON parse error: " << e.what() << "\n";
        return false;
    }

    auto& root = j["device_config"];

    config_.version = root.value("version", "");
    config_.description = root.value("description", "");

    auto& devs = root["devices"];

    parseCameras(devs);
    parsePLCDevices(devs);
    parseSensors(devs);
    parseGateways(devs);

    return true;
}

// ---------------- Cameras ----------------
void ConfigParser::parseCameras(const json& j)
{
    if (!j.contains("camera")) return;

    for (auto& item : j["camera"]) {
        CameraConfig c;
        c.id = item.value("id", "");
        c.name = item.value("name", "");
        c.url = item.value("url", "");
        config_.cameras.push_back(c);
    }
}

// ---------------- PLC Devices ----------------
void ConfigParser::parsePLCDevices(const json& j)
{
    if (!j.contains("plc_device")) return;

    for (auto& item : j["plc_device"]) {
        PLCDeviceConfig cfg;

        cfg.id = item.value("id", "");
        cfg.name = item.value("name", "");
        cfg.type = item.value("type", "");
        cfg.connectionType = item.value("connection_type", "");

        // direct
        if (cfg.connectionType == "direct" && item.contains("direct_config")) {
            cfg.hasDirect = true;

            auto& d = item["direct_config"];

            cfg.directConfig.serial.port = d["serial"].value("port", "");
            cfg.directConfig.serial.baudRate = d["serial"].value("baud_rate", 0);
            cfg.directConfig.serial.parity = d["serial"].value("parity", "");
            cfg.directConfig.serial.stopBits = d["serial"].value("stop_bits", 1);

            cfg.directConfig.plcRegister.address =
                d["plc_register"].value("address", "");
        }

        // gateway
        if (cfg.connectionType == "gateway" && item.contains("gateway_config")) {
            cfg.hasGateway = true;

            auto& g = item["gateway_config"];

            cfg.gatewayConfig.gatewayId = g.value("gateway_id", "");
            cfg.gatewayConfig.gatewayIp = g.value("gateway_ip", "");
            cfg.gatewayConfig.gatewayPort = g.value("gateway_port", 0);
            cfg.gatewayConfig.plcNodeId = g.value("plc_node_id", 0);
            cfg.gatewayConfig.plcRegister.address =
                g["plc_register"].value("address", "");
        }

        config_.plcDevices.push_back(cfg);
    }
}

// ---------------- Sensors ----------------
void ConfigParser::parseSensors(const json& j)
{
    if (!j.contains("sensor")) return;

    for (auto& item : j["sensor"]) {
        SensorConfig s;
        s.id = item.value("id", "");
        s.name = item.value("name", "");
        s.type = item.value("type", "");

        s.serial.port = item["serial_config"].value("port", "");
        s.serial.baudRate = item["serial_config"].value("baud_rate", 0);
        s.serial.parity = item["serial_config"].value("parity", "");
        s.serial.stopBits = item["serial_config"].value("stop_bits", 1);

        config_.sensors.push_back(s);
    }
}

// ---------------- Gateways ----------------
void ConfigParser::parseGateways(const json& j)
{
    if (!j.contains("gateway")) return;

    for (auto& item : j["gateway"]) {
        GatewayConfig g;
        g.id = item.value("id", "");
        g.name = item.value("name", "");
        g.model = item.value("model", "");
        g.ip = item.value("ip", "");
        g.protocol = item.value("protocol", "");
        g.status = item.value("status", "");

        config_.gateways.push_back(g);
    }
}
