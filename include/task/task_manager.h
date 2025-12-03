#ifndef DEVICE_TASK_MANAGER_H
#define DEVICE_TASK_MANAGER_H

#include "idevice_manager.h"
#include "thread_pool.h"
#include "camera_info.h"
#include "device_info.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <optional>

// 简化任务状态：仅保留成功、失败、阻塞
enum class TaskState {
    SUCCESS,  // 执行成功
    FAILED,   // 执行失败（业务异常/参数错误）
    BLOCKED   // 阻塞（设备管理器未初始化，无法执行）
};

// 简化任务结果：状态 + 错误信息（失败时用）
struct TaskResult {
    TaskState state = TaskState::BLOCKED;  // 默认阻塞（未执行）
    std::string error_msg;                 // 失败/阻塞原因
};

// 云端任务结构体（任务ID由云端传入）
struct CloudTask {
    enum class TaskType {
        TASK_GET_STATUS,        // 获取设备状态
        TASK_GET_REAL_IMAGE,    // 单个摄像头实时图
        TASK_GET_ALL_REAL_IMAGE,// 所有摄像头实时图
        TASK_GET_HISTORY_IMAGE, // 单个摄像头历史图
        TASK_GET_ALL_HISTORY_IMAGE, // 所有摄像头历史图
        TASK_OPERATE_CAMERA,    // 操作摄像头
        TASK_OPERATE_PLC,       // 操作PLC
        TASK_UPDATE_CONFIG,     // 更新配置
        TASK_UNKNOWN            // 未知任务
    };

    std::string task_id;               // 任务ID（必须由云端传入）
    TaskType task_type;                // 任务类型
    CameraStaticInfo camera_info;      // 摄像头信息（单个摄像头任务用）
    std::string device_id;             // 设备ID（PLC操作等用）
    std::string cmd;                   // 操作命令（PLC操作等用）
};

// 设备任务管理类（单例+简化逻辑）
class DeviceTaskManager {
private:
    DeviceTaskManager();
    ~DeviceTaskManager() = default;
    DeviceTaskManager(const DeviceTaskManager&) = delete;
    DeviceTaskManager& operator=(const DeviceTaskManager&) = delete;

    std::unique_ptr<thread_pool> thread_pool_;          // 线程池
    std::shared_ptr<IDeviceManager> device_manager_;    // 设备管理器（注入）
    std::unordered_map<std::string, TaskResult> task_results_; // 任务结果存储（key：云端任务ID）
    std::mutex mtx_;                                     // 线程安全锁

public:
    static DeviceTaskManager& getInstance();              // 单例获取
    void setDeviceManager(std::shared_ptr<IDeviceManager> device_manager); // 注入设备管理器
    std::string receiveCloudTask(const CloudTask& task);  // 接收云端任务（返回云端任务ID/空字符串表示失败）
    std::optional<TaskResult> queryTaskResult(const std::string& task_id); // 查询任务结果（传入云端任务ID）
};

// 辅助函数：状态/任务类型转字符串（便于日志）
std::string taskStateToString(TaskState state);
std::string taskTypeToString(CloudTask::TaskType type);

#endif // DEVICE_TASK_MANAGER_H