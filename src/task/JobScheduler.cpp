#include "JobScheduler.h"
#include "mqtt_service.h"
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
    }
    cv_.notify_one();
    return tcb->id;

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