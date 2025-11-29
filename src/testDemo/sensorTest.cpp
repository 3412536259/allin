#include <iostream>
#include "sensor_manager.h"

int main() {
    SensorManager manager;

    int value = 0;
    int status = 0;

    for (int id = 1; id <= 3; ++id) {
        manager.getSensorData(id, value, status);

        std::cout << "=== Sensor ID: " << id << " ===\n";
        std::cout << "Value : " << value << "\n";
        std::cout << "Status: " << (status == 1 ? "OK" : "ERROR") << "\n\n";
    }

    return 0;
}
