#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "mock_sensor.h"

class SensorManager {
private:
    MockSensor sensor; 

public:
    SensorManager();
    void getSensorData(int id, int &value, int &status);
};

#endif // SENSOR_MANAGER_H
