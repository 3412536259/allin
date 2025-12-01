#include "plc_manager.h"
#include <iostream>
#include <unistd.h>

static std::string stateToString(PLCState s) {
    switch (s) {
    case PLCState::ONLINE:  return "ONLINE";
    case PLCState::OFFLINE: return "OFFLINE";
    }
    return "UNKNOWN";
}

static std::string resultToString(OperateResult r) {
    switch (r) {
    case OperateResult::SUCCESS: return "SUCCESS";
    case OperateResult::FAILED:  return "FAILED";
    case OperateResult::TIMEOUT: return "TIMEOUT";
    }
    return "UNKNOWN";
}

int main() {
    PLCManager mgr;

    std::cout << "=== PLCManager Test Start ===\n";

    if (!mgr.start()) {
        std::cerr << "Failed to start PLCManager!\n";
        return 1;
    }

    std::cout << "\n--- Test getStatus(\"Valve1\") ---\n";
    auto info1 = mgr.getStatus("Valve1");
    std::cout << "Device: " << info1.id
              << " | Type: " << info1.type
              << " | State: " << stateToString(info1.state)
              << "\n";

    std::cout << "\n--- Test getStatus(\"PumpA\") ---\n";
    auto info2 = mgr.getStatus("PumpA");
    std::cout << "Device: " << info2.id
              << " | Type: " << info2.type
              << " | State: " << stateToString(info2.state)
              << "\n";

    std::cout << "\n--- Test getStatus(\"UnknownDevice\") ---\n";
    auto info3 = mgr.getStatus("UnknownDevice");
    std::cout << "Device: " << info3.id
              << " | Type: " << info3.type
              << " | State: " << stateToString(info3.state)
              << "\n";

    std::cout << "\n--- Test getAllStatus() ---\n";
    auto all = mgr.getAllStatus();
    for (auto& d : all) {
        std::cout << "Device: " << d.id
                  << " | Type: " << d.type
                  << " | State: " << stateToString(d.state)
                  << "\n";
    }

    std::cout << "\n--- Test operate(\"Valve1\", \"open\") ---\n";
    auto opR = mgr.operate("Valve1", "open");
    std::cout << "operate result = " << resultToString(opR) << "\n";

    sleep(2);

    std::cout << "\n--- Test operate(\"Valve1\", \"close\") ---\n";
    opR = mgr.operate("Valve1", "close");
    std::cout << "operate result = " << resultToString(opR) << "\n";

    auto infoAfter = mgr.getStatus("Valve1");
    std::cout << "After operate, Valve1:"
              << " | Type: " << infoAfter.type
              << " | State: " << stateToString(infoAfter.state)
              << "\n";

    std::cout << "\n=== PLCManager Test End ===\n";
    return 0;
}
