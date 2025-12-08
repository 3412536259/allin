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
#endif