#include "update_config.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>
#include <thread>

// -----------------------------------------------------------
// 辅助定义和工具函数
// -----------------------------------------------------------

// 假设 ConfigUpdater 内部硬编码使用的文件名
static const std::string TEMP_FILE_PATH_SIMULATED = "temp_test_local.json";
static const std::string CONFIG_FILE_PATH_SIMULATED = "config_test_local.json";

// --- 辅助函数：清理测试文件 ---
void cleanupTestFiles() {
    // 确保清理 ConfigUpdater 内部硬编码的文件路径
    std::remove(TEMP_FILE_PATH_SIMULATED.c_str());
    std::remove(CONFIG_FILE_PATH_SIMULATED.c_str());
}

// --- 辅助函数：测试报告 ---
void reportTest(const std::string& testName, bool success, const std::string& message = "") {
    if (success) {
        std::cout << "[PASS] " << testName << " succeeded.\n";
    } else {
        std::cerr << "[FAIL] " << testName << " FAILED. Error Message: " << message << "\n";
    }
    std::cout << "------------------------------------------------\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));
    cleanupTestFiles();
}

// -----------------------------------------------------------
// 测试用例定义
// -----------------------------------------------------------

// 1. 成功案例：所有校验通过并覆盖原文件
void testCase_Success(ConfigUpdater& updater) {
    std::string testName = "Test 1: Valid Configuration Update";
    std::cout << "\n--- " << testName << " ---\n";
    cleanupTestFiles(); // 确保环境干净

    std::string validConfig = R"({
  "device_config": {
    "version": "1.0",
    "description": "设备配置（含PLC直连/网关连接两种类型）",
    "box": "1",
    "devices": {
      "camera": [
        {
          "id": "10",
          "name": "camera1",
          "url": "rtsp://admin:Wlkjaqxy411@10.9.255.21:554/Streaming/Channels/101"
        },
        {
          "id": "2",
          "name": "camera2",
          "url": "rtsp://admin:Wlkjaqxy411@10.9.255.21:554/Streaming/Channels/201"
        }
      ],

      "plc_list": [
        {
          "plc_id": "plc_01",
          "name": "主控制PLC",
          "slave_id": "0x01",
          "connection_type": "direct",
          "serial": {
            "port": "/dev/ttyS4",
            "baud_rate": 9600,
            "parity": "none",
            "stop_bits": 1
          }
        },
        {
          "plc_id": "plc_02",
          "name": "网关PLC",
          "slave_id": "0x01",
          "connection_type": "gateway",
          "gateway": {
            "gateway_id": "gw_001",
            "gateway_ip": "192.168.10.253",
            "gateway_port": 31001
          }
        }
      ],
      "plc_device": [
        {
          "id": "plc_dev_001",
          "plc_id": "plc_01",
          "name": "进料电磁阀",
          "type": "solenoid_valve",
          "register": "0x0504"
        },
        {
          "id": "plc_dev_002",
          "plc_id": "plc_02",
          "name": "出料电磁阀",
          "type": "solenoid_valve",
          "register": "0x0504"
        }
      ],


      "sensor": [
        {
          "id": "sensor001",
          "name": "车间温湿度传感器",
          "type": "modbus",
          "serial_config": {
            "port": "/dev/ttyS5",
            "baud_rate": 9600,
            "parity": "none",
            "stop_bits": 1
          },
          "modbus_addr": 1,
          "reg_start": 0,
          "reg_count": 2
        },
        {
          "id": "sensor002",
          "name": "机房水浸传感器",
          "type": "gpio",
          "serial_config": {
            "port": "gpio18",
            "baud_rate": 0,
            "parity": "",
            "stop_bits": 0
          }
        },
        {
          "id": "sensor003",
          "name": "管道压力传感器",
          "type": "custom",
          "serial_config": {
            "port": "/dev/ttyUSB2",
            "baud_rate": 19200,
            "parity": "even",
            "stop_bits": 1
          }
        }
      ],
      "carcontrol": [
        {
        "id": "carcontrol001",
        "name": "小车控制器",
        "serial_config": {
          "port": "/dev/ttyS4",
          "baud_rate": 9600,
          "parity": "none",
          "stop_bits": 1
          },
        "send_window_ms": 600,
        "send_interval_ms": 80,
        "operate_timeout_ms": 1500
        }
      ],

      "gateway": [
        {
          "id": "gw_001",
          "name": "工业智能网关",
          "ip": "192.168.10.253",
          "protocol": "Modbus TCP", 
          "status": "online" 
        }
      ]
    }
  }
})";

    UpdateConfigResult result = updater.updateConfig(validConfig);
    
    // 检查结果和最终文件是否存在（证明原子覆盖成功）
    bool fileExists = (std::ifstream(CONFIG_FILE_PATH_SIMULATED.c_str()).good());

    reportTest(testName, result.isSuccess && fileExists, result.message);
}

// // 2. 失败案例：JSON 格式错误
// void testCase_InvalidJsonFormat(ConfigUpdater& updater) {
//     std::string testName = "Test 2: Invalid JSON Format";
//     std::cout << "\n--- " << testName << " ---\n";
//     cleanupTestFiles();

//     // 缺少闭合大括号
//     std::string invalidConfig = R"({"device_config": {"version": "1.1", "devices": {}"; 

//     UpdateConfigResult result = updater.updateConfig(invalidConfig);
    
//     // 检查结果和错误信息是否匹配
//     bool passed = !result.isSuccess && result.message.find("JSON Parse Error") != std::string::npos;
//     reportTest(testName, passed, result.message);
// }

// 3. 失败案例：ID 冲突
void testCase_DuplicateId(ConfigUpdater& updater) {
    std::string testName = "Test 3: Duplicate ID Conflict (PLC vs Camera)";
    std::cout << "\n--- " << testName << " ---\n";
    cleanupTestFiles();

    std::string duplicateIdConfig = R"({
        "device_config": {
            "version": "1.1",
            "box": "1",
            "devices": {
                "plc_list": [
                    {"plc_id": "ID_DUPLICATE", "name": "PLC1", "slave_id": "0x01", "connection_type": "direct", "serial": {"baud_rate": 9600}},
                ],
                "camera": [
                    {"id": "ID_DUPLICATE", "name": "Cam1", "url": "rtsp://user:pass@1.2.3.4/stream"}
                ]
            }
        }
    })";

    UpdateConfigResult result = updater.updateConfig(duplicateIdConfig);
    
    bool passed = !result.isSuccess && result.message.find("ID_DUPLICATE' is duplicated") != std::string::npos;
    reportTest(testName, passed, result.message);
}

// 4. 失败案例：业务逻辑错误 (PLC 设备引用了不存在的 PLC ID)
void testCase_BrokenPlcAssociation(ConfigUpdater& updater) {
    std::string testName = "Test 4: Broken PLC Association";
    std::cout << "\n--- " << testName << " ---\n";
    cleanupTestFiles();

    std::string brokenAssocConfig = R"({
        "device_config": {
            "version": "1.1",
            "box": "1",
            "devices": {
                "plc_list": [],
                "plc_device": [
                    {"id": "dev_002", "plc_id": "plc_999_MISSING", "name": "Valve"}
                ]
            }
        }
    })";

    UpdateConfigResult result = updater.updateConfig(brokenAssocConfig);
    
    bool passed = !result.isSuccess && result.message.find("refers to unknown plc_id 'plc_999_MISSING'") != std::string::npos;
    reportTest(testName, passed, result.message);
}

// 5. 失败案例：缺少必需字段 (PLC 缺少 slave_id)
void testCase_MissingRequiredField(ConfigUpdater& updater) {
    std::string testName = "Test 5: Missing Required Field (PLC slave_id)";
    std::cout << "\n--- " << testName << " ---\n";
    cleanupTestFiles();

    std::string missingFieldConfig = R"({
        "device_config": {
            "version": "1.1",
            "box": "1",
            "devices": {
                "plc_list": [
                    {"plc_id": "plc_001", "name": "MainPLC", "connection_type": "direct", "serial": {"baud_rate": 9600}} 
                    // ^--- Missing "slave_id"
                ]
            }
        }
    })";

    UpdateConfigResult result = updater.updateConfig(missingFieldConfig);
    
    bool passed = !result.isSuccess && result.message.find("Missing or invalid 'slave_id'") != std::string::npos;
    reportTest(testName, passed, result.message);
}

// 6. 失败案例：网关配置无效 (缺少IP)
void testCase_InvalidGateway(ConfigUpdater& updater) {
    std::string testName = "Test 6: Invalid Gateway Config (Missing IP)";
    std::cout << "\n--- " << testName << " ---\n";
    cleanupTestFiles();

    std::string invalidGwConfig = R"({
        "device_config": {
            "version": "1.1",
            "box": "1",
            "devices": {
                "gateway": [
                    {"id": "gw_002", "name": "BadGW", "protocol": "Modbus TCP"} 
                    // ^--- Missing "ip"
                ]
            }
        }
    })";

    UpdateConfigResult result = updater.updateConfig(invalidGwConfig);
    
    bool passed = !result.isSuccess && result.message.find("Missing or invalid 'ip' address") != std::string::npos;
    reportTest(testName, passed, result.message);
}


int main() {
    std::cout << "Starting ConfigUpdater Validation Tests...\n";
    
    // 初始化 ConfigUpdater
    ConfigUpdater updater;

    // 运行所有测试用例
    testCase_Success(updater);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    // testCase_InvalidJsonFormat(updater);
    testCase_DuplicateId(updater);
    testCase_BrokenPlcAssociation(updater);
    testCase_MissingRequiredField(updater);
    testCase_InvalidGateway(updater);

    std::cout << "\nAll ConfigUpdater Tests Finished.\n";

    // 最终清理
    cleanupTestFiles(); 
    
    return 0;
}