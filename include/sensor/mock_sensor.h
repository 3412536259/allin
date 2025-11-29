#ifndef MOCK_SENSOR_H
#define MOCK_SENSOR_H

class MockSensor {
public:
    MockSensor();
    void read(int id, int &value, int &status);
};

#endif // MOCK_SENSOR_H
