#ifndef TASK_H
#define TASK_H

#include "itask.h"
#include <iostream>
#include <thread>
#include "json.hpp"
class GetCameraRealImageTask : public ITask
{
public:
    GetCameraRealImageTask(std::string camId)
        :camId_(camId){}
    std::string name() const override{return "GetCameraRealImage";}
    void run(TaskContext& ctx)override;

private:
    std::string camId_;
}; 

class OperateValveTask : public ITask
{
public:
    OperateValveTask(std::string deviceId, std::string cmd)
        : deviceId_(deviceId), cmd_(cmd){}
    std::string name() const override { return "OperateValve"; }
    void run(TaskContext& ctx) override;
private:
    std::string deviceId_;
    std::string cmd_;
};

class GetPLCDeviceTask : public ITask
{
public:
    GetPLCDeviceTask(std::string deviceId)
        : deviceId_(deviceId) {}
    std::string name() const override { return "GetPLCDeviceStatus"; }
    void run(TaskContext& ctx) override;
private:
    std::string deviceId_;
};

class GetSensorDataTask : public ITask
{
public:
    GetSensorDataTask(std::string sensorId)
        : sensorId_(sensorId) {}
    std::string name() const override { return "GetSensorData"; }
    void run(TaskContext& ctx) override;
private:
    std::string sensorId_;
};

#endif