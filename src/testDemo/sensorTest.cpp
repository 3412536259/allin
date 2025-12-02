#include "sensor_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    SensorManager mgr;
    if (!mgr.start("../../include/common/config/config.json", 5)) {
        std::cerr << "Failed to start SensorManager\n";
        return 1;
    }

    // 演示：实时读取某个传感器
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto r = mgr.getSensorDataRealTime("sensor_002");
    if (r) {
        std::cout << "Realtime sensor_002 -> temp=" << r->temperature
                  << " C, hum=" << r->humidity << " %, status=" << to_string(r->status) << "\n";
    } else {
        std::cout << "sensor_002 not found\n";
    }

    // 演示：读取缓存
    auto c = mgr.getSensorDataCached("sensor_001");
    if (c) {
        std::cout << "Cached sensor_001 -> temp=" << c->temperature
                  << " C, hum=" << c->humidity << " %, status=" << to_string(c->status) << "\n";
    } else {
        std::cout << "sensor_001 not in cache\n";
    }

    // 等待一会儿让后台刷新跑几次
    std::this_thread::sleep_for(std::chrono::seconds(12));

    mgr.stop();
    return 0;
}
