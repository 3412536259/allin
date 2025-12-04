#include "JobScheduler.h"
JobScheduler::JobScheduler(size_t workerCount)
    :pool_(workerCount)
{
    dispatcher_ = std::thread(&JobScheduler::dispatchLoop,this);
}

JobScheduler::~JobScheduler()
{
    stop_ = true;
    cv_.notify_all();
    if(dispatcher_.joinable()) dispatcher_.join();
}

int JobScheduler::submit(std::shared_ptr<ITask> task)
{  
    auto tcb = std::make_shared<TaskControlBlock>();
    tcb->id = nextId_++;
    tcb->task = task;
    tcb->status = TaskStatus::READY;
    tcb->name = task->name();
    tcb->enqueueTime = std::chrono::steady_clock::now();
   
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
            TaskContext ctx{.taskId = tcb->id};
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