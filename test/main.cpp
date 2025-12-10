#include "plc_manager.h"
#include <iostream>
#include <vector>
#include <string>
#include <thread> // 引入线程和休眠
#include <chrono> // 引入时间
#include <algorithm> // 引入 std::find_if
#include "config_parser.h"
#include "config_info.h" // 引入 PLCDeviceStatus 定义

int main() {
    // 假设配置路径正确，并加载配置
    ConfigParser::getInstance().loadFromFile("../../include/common/config/config.json");
    
    // 1. 初始化 PLC 管理器：管理器内部会加载配置、初始化设备、启动连接并启动定时刷新线程 (5s)
    PLCManager manager;

    // 简单的检查 PLC 是否加载成功
    if (manager.getAllStatus().plcList.empty()) {
        std::cerr << "Manager initialization failed or no PLCs found. Exiting application.\n";
    }

    // 2. 持续操作测试流程 (连续操作 plc_dev_001 10 次)
    std::cout << "\n--- 持续操作测试: 连续操作 plc_dev_001 并检查状态 ---\n";
    std::cout << "注意: 每次操作后会立即查询状态 (缓存被清除)，以验证操作结果。\n";

    const std::string deviceId = "plc_dev_002";
    const std::string plcId = "plc_002";
    int iterations = 10; // 执行 10 次操作

    for (int i = 0; i < iterations; ++i) {
        // 确定当前操作命令：偶数次操作 ON，奇数次操作 OFF
        std::string command = (i % 2 == 0) ? "ON" : "OFF";
        
        std::cout << "\n--- 循环 " << i + 1 << " / " << iterations << " ---\n";

        // 执行操作
        OperateResult opResult = manager.operate(deviceId, command);
        std::cout << "操作 [" << command << "] - 结果: " 
                  << (opResult.success ? "成功" : "失败") 
                  << ". 消息: " << opResult.message << std::endl;

        // 立即查询状态 (由于 manager.operate 清除了缓存，getStatus 会强制实时读取)
        PLCInfo status = manager.getStatus(deviceId);
        
        std::string deviceStatus = "N/A";
        
        // **修正**：通过 std::find_if 在 vector (deviceStatuses) 中查找对应的设备状态
        auto itDeviceStatus = std::find_if(status.deviceStatuses.begin(), status.deviceStatuses.end(), 
                                           [&deviceId](const PLCDeviceStatus& ds) {
                                               return ds.id == deviceId;
                                           });

        if (itDeviceStatus != status.deviceStatuses.end()) {
            deviceStatus = itDeviceStatus->status;
        } else {
             std::cerr << "WARNING: Device ID " << deviceId << " not found in status list for PLC " << plcId << ".\n";
        }

        std::cout << "-> PLC " << plcId << " 连接状态: " << status.connectionStatus 
                  << ", 设备 " << deviceId << " 实时状态: [" << deviceStatus << "]" << std::endl;
        
        // 暂停 1 秒，以便观察两次操作之间的间隔
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // 3. 线程退出前最后一次查询，确保定时线程已运行
    std::cout << "\n--- 操作循环结束。等待 5 秒，确保定时刷新线程至少执行一次... ---\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    std::cout << "\n--- 最终状态查询 (可能由定时线程刷新) ---\n";
    PLCInfo finalStatus = manager.getStatus(deviceId);
    std::string finalDeviceStatus = "N/A";
    
    // **修正**：通过 std::find_if 在 vector (deviceStatuses) 中查找最终设备状态
    auto itFinalDeviceStatus = std::find_if(finalStatus.deviceStatuses.begin(), finalStatus.deviceStatuses.end(), 
                                       [&deviceId](const PLCDeviceStatus& ds) {
                                           return ds.id == deviceId;
                                       });

    if (itFinalDeviceStatus != finalStatus.deviceStatuses.end()) {
        finalDeviceStatus = itFinalDeviceStatus->status;
    } else {
         std::cerr << "WARNING: Device ID " << deviceId << " not found in final status list for PLC " << plcId << ".\n";
    }
    
    std::cout << "最终状态 - PLC " << plcId << " 连接状态: " << finalStatus.connectionStatus 
              << ", 设备 " << deviceId << " 状态: [" << finalDeviceStatus << "]" << std::endl;

    std::cout << "\n测试完成。PLCManager 析构函数将停止定时线程并断开连接。\n";

    return 0;
}