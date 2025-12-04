#include "plc_manager.h"
#include <iostream>
#include <vector>
#include "config_parser.h"

int main() {
    ConfigParser::getInstance().loadFromFile("../include/common/config/config.json");
    // 1. 初始化 PLC 管理器：管理器内部会加载和解析配置
    // 如果加载失败，PLCManager 内部会打印错误消息，但程序会继续运行（配置为空）。
    // 在实际生产代码中，这里应该捕获异常或检查初始化状态。
    PLCManager manager;

    // 简单的检查 PLC 是否加载成功
    // 假设 manager.getAllStatus() 如果没有加载任何 PLC，会返回空列表
    if (manager.getAllStatus().empty()) {
        std::cerr << "Manager initialization failed or no PLCs found. Exiting application.\n";
        // 注意：由于 PLCManager 构造函数不返回状态，这里需要一个额外的状态检查
        // 为了演示目的，我们继续运行，但应警惕空配置问题。
    }

    // 2. 执行测试流程

    std::cout << "\n--- 1. 第一次查询 (缓存失效/未命中) ---\n";
    PLCInfo status1 = manager.getStatus("plc_01");
    std::cout << "PLC 01 Status: " << status1.connectionStatus 
              << ", Device 001 Status: " << (status1.deviceStatuses.empty() ? "N/A" : status1.deviceStatuses[0].status) << std::endl;

    std::cout << "\n--- 2. 第二次查询 (缓存命中) ---\n";
    PLCInfo status2 = manager.getStatus("plc_01");
    std::cout << "PLC 01 Status: " << status2.connectionStatus 
              << ", Device 001 Status: " << (status2.deviceStatuses.empty() ? "N/A" : status2.deviceStatuses[0].status) << std::endl;

    std::cout << "\n--- 3. 执行操作 ---\n";
    OperateResult opResult = manager.operate("plc_dev_001", "OFF");
    std::cout << "Operate Result: " << (opResult.success ? "Success" : "Failed") 
              << ". Message: " << opResult.message << std::endl;

    std::cout << "\n--- 4. 第三次查询 (缓存因操作被清除而失效) ---\n";
    PLCInfo status3 = manager.getStatus("plc_01");
    std::cout << "PLC 01 Status: " << status3.connectionStatus 
              << ", Device 001 Status: " << (status3.deviceStatuses.empty() ? "N/A" : status3.deviceStatuses[0].status) << std::endl;

    std::cout << "\n--- 5. 查询所有状态 ---\n";
    std::vector<PLCInfo> allStatus = manager.getAllStatus();
    for (const auto& info : allStatus) {
        std::cout << "All Status - PLC " << info.plcId << ": " << info.connectionStatus << std::endl;
    }

    return 0;
}