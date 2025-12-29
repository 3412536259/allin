#include "JobScheduler.h"
#include "json.hpp"
#include "task.h" 
#include "logger.h"
#include "mqtt_topics.h"
JobScheduler::JobScheduler(size_t workerCount,IDeviceManager* devMgr/*,ITaskResultPublisher* publisher*/)
    :pool_(workerCount),devMgr_(devMgr)/*,publisher_(publisher)*/
{
    dispatcher_ = std::thread(&JobScheduler::dispatchLoop,this);
}

JobScheduler::~JobScheduler()
{
    stop_ = true;
    cv_.notify_all();
    if(dispatcher_.joinable()) dispatcher_.join();
}
// void JobScheduler::setPublisher(ITaskResultPublisher* publisher)
// {
//     publisher_ = publisher;
// }
int JobScheduler::submit(std::shared_ptr<ITask> task, const std::string& source)
{  
    auto tcb = std::make_shared<TaskControlBlock>();
    tcb->id = nextId_++;
    tcb->task = task;
    tcb->status = TaskStatus::READY;
    tcb->name = task->name();
    tcb->enqueueTime = std::chrono::steady_clock::now();
    tcb->source = source;
   
    {
        std::lock_guard<std::mutex> lk(mtx_); 
        readyQueue_.push(tcb);
        taskTable_[tcb->id] = tcb;
        // if this is a CarControlTask, record it as the latest for that carId
        auto carTask = std::dynamic_pointer_cast<CarControlTask>(task);
        if (carTask) {
            std::string cid = carTask->getCarId();
            std::lock_guard<std::mutex> lock(carControlMtx_);
            latestCarControlTask_[cid] = tcb;
        }
    }
    cv_.notify_one();
    return tcb->id;

}




void JobScheduler::cancelCarControl(const std::string& carId) {
    // 转发到设备管理器，由设备层负责中断/取消正在执行的操作
    if (devMgr_) {
        try { devMgr_->cancelCarControl(carId); } catch(...) {}
    }
} 



TaskStatus JobScheduler::getTaskStatus(int taskId)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if(taskTable_.count(taskId))
    {
        return taskTable_[taskId]->status;
    }
    return TaskStatus::FAILED;
}

void JobScheduler::dispatchLoop()
{
    while(!stop_)
    {
        std::shared_ptr<TaskControlBlock> tcb = nullptr;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk,[this]{
                return stop_ || !readyQueue_.empty();
            });

            if(stop_) break;

            tcb = readyQueue_.front();
            readyQueue_.pop();

            tcb->status = TaskStatus::RUNNING;
            tcb->startTime = std::chrono::steady_clock::now();
            runningSet_.insert(tcb->id);
        }

        pool_.submit([this,tcb](){
            ITaskResultPublisher* pub = (tcb->source == "http") ? httpPublisher_ : mqttPublisher_;
            TaskContext ctx{.taskId = tcb->id,.devMgr = this->devMgr_,.publisher = pub,.source = tcb->source};
            
             auto carControlTask = std::dynamic_pointer_cast<CarControlTask>(tcb->task);
                if (carControlTask) {
                    std::string carId = carControlTask->getCarId();
                    
                    // 检查此任务是否仍是该小车的最新任务
                    bool isLatest = true;
                    {
                        std::lock_guard<std::mutex> lock(carControlMtx_);
                        auto it = latestCarControlTask_.find(carId);
                        if (it != latestCarControlTask_.end()) {
                            auto latestTask = it->second.lock();
                            if (latestTask && latestTask->id != tcb->id) {
                                // 任务已被覆盖
                                isLatest = false;
                            }
                        }
                    }
                    
                    if (!isLatest) {
                        // 任务已被覆盖，视为提前正常结束：返回与正常完成一致的成功结果
                        nlohmann::json result;
                        result["success"] = true;
                        result["carcontrolId"] = carId;
                        // 从任务中读取命令参数并回传
                        int motor1 = carControlTask->getMotor1();
                        int motor2 = carControlTask->getMotor2();
                        result["motor1"] = motor1;
                        result["motor2"] = motor2;

                        int last = -1;
                        try { last = devMgr_->getCarControlLastStatus(carId); } catch(...) { last = -1; }

                        int statusOut = 0;
                        if (last == -1) {
                            statusOut = -1;
                        } else if (last == 0) {
                            statusOut = 0;
                        } else if (last == 257) {
                            statusOut = 1;
                        } else if (last == 514) {
                            statusOut = 2;
                        } else {
                            statusOut = 0;
                        }
                        result["status"] = statusOut;

                        pub->publish(RESULT_OPERATE_CAR_TOPIC, result.dump());
                        tcb->status = TaskStatus::FINISHED;
                        return;
                    }
                }


            try{
                tcb->task->run(ctx);
                tcb->status = TaskStatus::FINISHED;
            }catch(...){
                tcb->status = TaskStatus::FAILED;
            }
            {
                std::lock_guard<std::mutex> lk(this->mtx_);
                tcb->duration = std::chrono::steady_clock::now() - tcb->startTime;
                runningSet_.erase(tcb->id);
            }

        });
    }
}