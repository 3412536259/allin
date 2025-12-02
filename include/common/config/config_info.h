#pragma once
#include <string>
#include <vector>

// -------- Camera --------
struct CameraConfig {
    std::string id;
    std::string name;
    std::string url;
};

// -------- Serial --------
struct SerialConfig {
    std::string port;
    int baudRate = 0;
    std::string parity;
    int stopBits = 1;
};

// -------- PLC Register --------
struct PLCRegister {
    std::string address;
};

// -------- PLC Direct Config --------
struct DirectPLCConfig {
    SerialConfig serial;
    PLCRegister plcRegister;
};

// -------- PLC Gateway Config --------
struct GatewayPLCConfig {
    std::string gatewayId;
    std::string gatewayIp;
    int gatewayPort = 0;
    int plcNodeId = 0;
    PLCRegister plcRegister;
};

// -------- PLC Device --------
struct PLCDeviceConfig {
    std::string id;
    std::string name;
    std::string type;  // solenoid_valve
    std::string connectionType; // direct / gateway

    DirectPLCConfig directConfig;
    GatewayPLCConfig gatewayConfig;
    bool hasDirect = false;
    bool hasGateway = false;
};

// -------- Sensor --------
struct SensorConfig {
    std::string id;
    std::string name;
    std::string type;
    SerialConfig serial;
};

// -------- Gateway --------
struct GatewayConfig {
    std::string id;
    std::string name;
    std::string model;
    std::string ip;
    std::string protocol;
    std::string status;
};

// -------- Root Config --------
struct DeviceConfigRoot {
    std::string version;
    std::string description;

    std::vector<CameraConfig> cameras;
    std::vector<PLCDeviceConfig> plcDevices;
    std::vector<SensorConfig> sensors;
    std::vector<GatewayConfig> gateways;
};

struct PLCConfig{
    std::string id;
    std::string name;
    std::string type;  // solenoid_valve
    std::string connectionType; // direct / gateway

    bool hasDirect = false;
    DirectPLCConfig directConfig;

    bool hasGateway = false;
    GatewayPLCConfig gatewayConfig;
};