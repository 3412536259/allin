#include "sensor_manager.h"

SensorManager::SensorManager() : sensor() {}

void SensorManager::getSensorData(int id, int &value, int &status) {
    sensor.read(id, value, status);
}
