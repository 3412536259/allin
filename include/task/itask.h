#ifndef I_TASK_H
#define I_TASK_H
#include <string>
#include <future>
#include <chrono>
#include <functional>
#include "idevice_manager.h"
enum class TaskResult
{
    SUCCESS,
    TIMEOUT,
    FAILED
    
};

class ITask
{
public:
    virtual ~ITask() = default;
    virtual TaskResult execute(IDeviceManager& devMgr) = 0;
    virtual std::string name() const = 0;

};

// ------------------------
// 超时包装器
// ------------------------
template<typename F>
auto make_timeout_task(F task, std::chrono::milliseconds timeout)
{
    return [task, timeout]() {
        using R = TaskResult;

        std::packaged_task<R()> pkg(task);
        std::future<R> fut = pkg.get_future();

        // 在独立线程执行真正的任务
        std::thread(std::move(pkg)).detach();

        if (fut.wait_for(timeout) == std::future_status::timeout)
        {
            std::cout << "[Task] Timeout!" << std::endl;
            return TaskResult::TIMEOUT;
        }
        return fut.get();
    };
}

#endif