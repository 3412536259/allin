#include <iostream>
#include "config_parser.h"

void printLine() {
    std::cout << "---------------------------------------------\n";
}

int main() {
    ConfigParser parser;
    if (!parser.loadFromFile("config.json")) {
        std::cerr << "Failed to load config.json\n";
        return -1;
    }

    const auto& cfg = parser.getConfig();

    printLine();
    std::cout << "Config Version: " << cfg.version << "\n";
    std::cout << "Description   : " << cfg.description << "\n";
    printLine();


    // ---------------- Cameras ----------------
    std::cout << "Cameras (" << cfg.cameras.size() << "):\n";
    for (const auto& cam : cfg.cameras) {
        std::cout << "  ID   : " << cam.id << "\n";
        std::cout << "  Name : " << cam.name << "\n";
        std::cout << "  URL  : " << cam.url << "\n";
        printLine();
    }


    // ---------------- PLC Devices ----------------
    std::cout << "PLC Devices (" << cfg.plcDevices.size() << "):\n";
    for (const auto& plc : cfg.plcDevices) {
        std::cout << "  ID   : " << plc.id << "\n";
        std::cout << "  Name : " << plc.name << "\n";
        std::cout << "  Type : " << plc.type << "\n";
        std::cout << "  Connection Type: " << plc.connectionType << "\n";

        if (plc.hasDirect) {
            std::cout << "  --- Direct Config ---\n";
            std::cout << "      Serial Port : " << plc.directConfig.serial.port << "\n";
            std::cout << "      Baud Rate   : " << plc.directConfig.serial.baudRate << "\n";
            std::cout << "      Parity      : " << plc.directConfig.serial.parity << "\n";
            std::cout << "      Stop Bits   : " << plc.directConfig.serial.stopBits << "\n";
            std::cout << "      Register    : " << plc.directConfig.plcRegister.address << "\n";
        }

        if (plc.hasGateway) {
            std::cout << "  --- Gateway Config ---\n";
            std::cout << "      Gateway ID  : " << plc.gatewayConfig.gatewayId << "\n";
            std::cout << "      IP          : " << plc.gatewayConfig.gatewayIp << "\n";
            std::cout << "      Port        : " << plc.gatewayConfig.gatewayPort << "\n";
            std::cout << "      PLC Node ID : " << plc.gatewayConfig.plcNodeId << "\n";
            std::cout << "      Register    : " << plc.gatewayConfig.plcRegister.address << "\n";
        }

        printLine();
    }


    // ---------------- Sensors ----------------
    std::cout << "Sensors (" << cfg.sensors.size() << "):\n";
    for (const auto& s : cfg.sensors) {
        std::cout << "  ID   : " << s.id << "\n";
        std::cout << "  Name : " << s.name << "\n";
        std::cout << "  Type : " << s.type << "\n";
        std::cout << "    Serial Port : " << s.serial.port << "\n";
        std::cout << "    Baud Rate   : " << s.serial.baudRate << "\n";
        std::cout << "    Parity      : " << s.serial.parity << "\n";
        std::cout << "    Stop Bits   : " << s.serial.stopBits << "\n";
        printLine();
    }


    // ---------------- Gateways ----------------
    std::cout << "Gateways (" << cfg.gateways.size() << "):\n";
    for (const auto& g : cfg.gateways) {
        std::cout << "  ID       : " << g.id << "\n";
        std::cout << "  Name     : " << g.name << "\n";
        std::cout << "  Model    : " << g.model << "\n";
        std::cout << "  IP       : " << g.ip << "\n";
        std::cout << "  Protocol : " << g.protocol << "\n";
        std::cout << "  Status   : " << g.status << "\n";
        printLine();
    }

    std::cout << "All config printed successfully.\n";
    return 0;
}
