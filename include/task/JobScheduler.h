#ifndef JOB_SCHEDULER_H
#define JOB_SCHEDULER_H

#include "itask.h"
#include "thread_pool.h"
#include "idevice_manager.h"
#include <queue>
#include <mutex>
#include <thread>
#include <set>
#include <condition_variable>
#include <map>
#include "itask_result_publisher.h"
class JobScheduler{
public:
    JobScheduler(size_t workerCount,IDeviceManager* devMgr,ITaskResultPublisher* publisher);
    ~JobScheduler();

    // submit task; source can be "mqtt" or "http" (default "mqtt")
    int submit(std::shared_ptr<ITask> task, const std::string& source = "mqtt");
    void setPublisher(ITaskResultPublisher* publisher);
    TaskStatus getTaskStatus(int taskId);
private:
    void dispatchLoop();

private:
    ThreadPool pool_;
    std::thread dispatcher_;

    std::mutex mtx_;
    std::condition_variable cv_;

    std::queue<std::shared_ptr<TaskControlBlock>> readyQueue_;
    std::set<int> runningSet_;
    std::map<int, std::shared_ptr<TaskControlBlock>> taskTable_;

    std::atomic_bool stop_{false};
    std::atomic_int nextId_{1};
    IDeviceManager* devMgr_;
    ITaskResultPublisher* publisher_;
};



#endif