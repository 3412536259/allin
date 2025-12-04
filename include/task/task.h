#ifndef TASK_H
#define TASK_H

#include "itask.h"
#include <iostream>
#include <thread>
class GetCameraRealImageTask : public ITask
{
public:
    GetCameraRealImageTask(std::string camId)
        :camId_(camId){}
    std::string name() const override{return "GetCameraRealImage";}
    void run(TaskContext& ctx)override
    {
        
        std::cout << ctx.taskId << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
private:
    std::string camId_;
};

#endif