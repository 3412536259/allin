#ifndef I_TASK_H
#define I_TASK_H
#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include "idevice_manager.h"
#include "itask_result_publisher.h"
class MqttService; 
class TaskContext{
public:
    int taskId;
    IDeviceManager* devMgr;
    ITaskResultPublisher* publisher;
};

class ITask{
public:
    virtual ~ITask() = default;
    virtual void run(TaskContext& ctx) = 0;
    virtual std::string name() const = 0;
};

enum class TaskStatus {
    NEW,
    READY,
    RUNNING,
    FINISHED,
    FAILED
};

struct TaskControlBlock{
    int id;
    std::shared_ptr<ITask> task;
    TaskStatus status = TaskStatus::NEW;

    std::chrono::steady_clock::time_point enqueueTime;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::duration duration{};

    std::string name;
};






#endif