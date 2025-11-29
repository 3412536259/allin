#include "mock_sensor.h"
#include <cstdlib>
#include <ctime>

MockSensor::MockSensor() {
    std::srand(std::time(nullptr));
}

void MockSensor::read(int id, int &value, int &status) {
    // 模拟不同 ID 不同范围
    switch(id) {
        case 1:
            value = std::rand() % 50;  // 0~49
            break;
        case 2:
            value = 50 + std::rand() % 50; // 50~99
            break;
        default:
            value = std::rand() % 100;
            break;
    }

    // 设置状态
    status = (value > 20) ? 1 : 0;
}
