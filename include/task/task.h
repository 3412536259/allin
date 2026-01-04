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


class OperateValueWithVerifyTask : public ITask
{
public:
    OperateValueWithVerifyTask(std::string deviceId, std::string cmd, std::string sensorId, std::string cameraId)
        : deviceId_(deviceId), cmd_(cmd), sensorId_(sensorId), cameraId_(cameraId){}
    std::string name() const override { return "OperateValueWithVerify";}
    void run(TaskContext& ctx) override;
private:
    std::string deviceId_;
    std::string cmd_;
    std::string sensorId_;
    std::string cameraId_;
};
/*
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
*/
class CarControlTask : public ITask
{
public:
    CarControlTask(const nlohmann::json& payload)
        : payload_(payload) {
            carId_ = payload_.value("carcontrolId", std::string("carcontrol001"));
        }
    std::string name() const override { return "CarControlTask"; }
    void run(TaskContext& ctx) override;
    std::string getCarId() const { return carId_; }
    int getMotor1() const { return payload_.value("motor1", 0); }
    int getMotor2() const { return payload_.value("motor2", 0); }
private:
    nlohmann::json payload_;
    std::string carId_; // 添加carId成员变量
    // 添加辅助方法
    bool validatePayload(const nlohmann::json& payload);
    void publishResult(ITaskResultPublisher* publisher, const nlohmann::json& result);
};

class GetDeviceStatusTask : public ITask
{
public:
    GetDeviceStatusTask() {}
    std::string name() const override { return "GetDeviceStatus";}
    void run(TaskContext& ctx) override;
};

class UpdateConfigTask : public ITask
{
public:
    UpdateConfigTask(const std::string& JsonStr) : JsonStr_(JsonStr) {}
    std::string name() const override { return "UpdateConfig"; }
    void run(TaskContext& ctx) override;
private:
    std::string JsonStr_;
};

// class DownloadVideoTask : public ITask
// {
// public:
//     DownloadVideoTask(std::string channel, std::string date, std::string time)
//         : channel_(channel), date_(date), time_(time) {}
//     std::string name() const override { return "DownloadVideoTask"; }
//     void run(TaskContext& ctx) override;
// private:
//     std::string channel_;
//     std::string date_;
//     std::string time_;
// };
#endif