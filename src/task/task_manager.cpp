#include "task_manager.h"
#include <iostream>

// 单例初始化
DeviceTaskManager::DeviceTaskManager() 
    : thread_pool_(std::make_unique<thread_pool>()) {
    std::cout << "[DeviceTaskManager] Initialized" << std::endl;
}

DeviceTaskManager& DeviceTaskManager::getInstance() {
    static DeviceTaskManager instance;
    return instance;
}

// 注入设备管理器
void DeviceTaskManager::setDeviceManager(std::shared_ptr<IDeviceManager> device_manager) {
    std::lock_guard<std::mutex> lock(mtx_);
    device_manager_ = std::move(device_manager);
    std::cout << "[DeviceTaskManager] DeviceManager injected" << std::endl;
}

// 接收云端任务（任务ID由云端传入，空ID直接拒绝）
std::string DeviceTaskManager::receiveCloudTask(const CloudTask& task) {
    // 校验：云端未传入任务ID，直接拒绝
    if (task.task_id.empty()) {
        std::cerr << "[DeviceTaskManager] Reject task: Cloud task ID is empty" << std::endl;
        return ""; // 返回空字符串表示接收失败
    }

    // 校验：同一任务ID重复提交（可选，根据业务需求决定是否允许）
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (task_results_.count(task.task_id) > 0) {
            std::cerr << "[DeviceTaskManager] Reject task: Duplicate task ID=" << task.task_id << std::endl;
            return "";
        }
    }

    // 初始化任务结果（默认阻塞）
    {
        std::lock_guard<std::mutex> lock(mtx_);
        task_results_[task.task_id] = {TaskState::BLOCKED, "Task not executed yet"};
    }

    std::cout << "[DeviceTaskManager] Received cloud task: ID=" << task.task_id 
              << ", Type=" << taskTypeToString(task.task_type) << std::endl;

    // 提交到线程池异步执行
    thread_pool_->submit([this, task]() {
        TaskResult result;
        try {
            // 检查设备管理器是否初始化（阻塞判断）
            std::shared_ptr<IDeviceManager> device_manager;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                device_manager = device_manager_;
            }

            if (!device_manager) {
                result.state = TaskState::BLOCKED;
                result.error_msg = "Blocked: DeviceManager not initialized";
                throw std::runtime_error(result.error_msg);
            }

            // 执行对应设备接口
            switch (task.task_type) {
                case CloudTask::TaskType::TASK_GET_STATUS:
                    device_manager->getStatus();
                    break;
                case CloudTask::TaskType::TASK_GET_REAL_IMAGE:
                    device_manager->getRealImage(task.camera_info);
                    break;
                case CloudTask::TaskType::TASK_GET_ALL_REAL_IMAGE:
                    device_manager->getAllRealImage();
                    break;
                case CloudTask::TaskType::TASK_GET_HISTORY_IMAGE:
                    device_manager->getHistoryImage(task.camera_info);
                    break;
                case CloudTask::TaskType::TASK_GET_ALL_HISTORY_IMAGE:
                    device_manager->getAllHistoryImage();
                    break;
                case CloudTask::TaskType::TASK_OPERATE_CAMERA:
                    device_manager->operateCamera();
                    break;
                case CloudTask::TaskType::TASK_OPERATE_PLC:
                    device_manager->operatePlc(task.device_id, task.cmd);
                    break;
                case CloudTask::TaskType::TASK_UPDATE_CONFIG:
                    device_manager->updateConfig();
                    break;
                default:
                    throw std::runtime_error("Unknown task type");
            }

            // 执行成功
            result.state = TaskState::SUCCESS;
            result.error_msg = "";
            std::cout << "[Task] Success: Cloud ID=" << task.task_id << std::endl;

        } catch (const std::exception& e) {
            // 执行失败（包含阻塞情况）
            result.state = (e.what() == std::string("Blocked: DeviceManager not initialized")) 
                          ? TaskState::BLOCKED : TaskState::FAILED;
            result.error_msg = e.what();
            std::cerr << "[Task] " << taskStateToString(result.state) 
                      << ": Cloud ID=" << task.task_id << ", Reason=" << e.what() << std::endl;
        }

        // 更新任务结果（线程安全）
        {
            std::lock_guard<std::mutex> lock(mtx_);
            task_results_[task.task_id] = result;
        }
    });

    return task.task_id; // 返回云端任务ID，表示接收成功
}

// 查询任务结果（传入云端任务ID）
std::optional<TaskResult> DeviceTaskManager::queryTaskResult(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = task_results_.find(task_id);
    if (it != task_results_.end()) {
        return it->second;
    }
    return std::nullopt; // 任务ID不存在（未接收过该云端任务）
}

// 辅助函数：TaskState转字符串
std::string taskStateToString(TaskState state) {
    switch (state) {
        case TaskState::SUCCESS: return "SUCCESS";
        case TaskState::FAILED: return "FAILED";
        case TaskState::BLOCKED: return "BLOCKED";
        default: return "UNKNOWN";
    }
}

// 辅助函数：TaskType转字符串
std::string taskTypeToString(CloudTask::TaskType type) {
    switch (type) {
        case CloudTask::TaskType::TASK_GET_STATUS: return "GET_STATUS";
        case CloudTask::TaskType::TASK_GET_REAL_IMAGE: return "GET_REAL_IMAGE";
        case CloudTask::TaskType::TASK_GET_ALL_REAL_IMAGE: return "GET_ALL_REAL_IMAGE";
        case CloudTask::TaskType::TASK_GET_HISTORY_IMAGE: return "GET_HISTORY_IMAGE";
        case CloudTask::TaskType::TASK_GET_ALL_HISTORY_IMAGE: return "GET_ALL_HISTORY_IMAGE";
        case CloudTask::TaskType::TASK_OPERATE_CAMERA: return "OPERATE_CAMERA";
        case CloudTask::TaskType::TASK_OPERATE_PLC: return "OPERATE_PLC";
        case CloudTask::TaskType::TASK_UPDATE_CONFIG: return "UPDATE_CONFIG";
        default: return "UNKNOWN";
    }
}