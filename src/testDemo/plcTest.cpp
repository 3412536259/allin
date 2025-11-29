#include "plc_manager.h"
#include <iostream>

static std::string stateToString(PLCState s) {
    return (s == PLCState::ONLINE) ? "ONLINE" : "OFFLINE";
}

int main() {
    PLCManager mgr;

    std::cout << "=== PLCManager Test Start ===" << std::endl;

    if (!mgr.start()) {
        std::cerr << "Failed to start PLCManager!" << std::endl;
        return 1;
    }

    std::cout << "\n--- Test getStatus(\"Valve1\") ---\n";
    auto info1 = mgr.getStatus("Valve1");
    std::cout << "Device: " << info1.id
              << " | State: " << stateToString(info1.state) << std::endl;

    std::cout << "\n--- Test getStatus(\"PumpA\") ---\n";
    auto info2 = mgr.getStatus("PumpA");
    std::cout << "Device: " << info2.id
              << " | State: " << stateToString(info2.state) << std::endl;

    std::cout << "\n--- Test getStatus(\"UnknownDevice\") ---\n";
    auto info3 = mgr.getStatus("UnknownDevice");
    std::cout << "Device: " << info3.id
              << " | State: " << stateToString(info3.state) << std::endl;

    std::cout << "\n--- Test getAllStatus() ---\n";
    auto all = mgr.getAllStatus();
    for (auto& d : all) {
        std::cout << "Device: " << d.id
                  << " | State: " << stateToString(d.state)
                  << std::endl;
    }

    std::cout << "\n=== PLCManager Test End ===" << std::endl;
    return 0;
}
