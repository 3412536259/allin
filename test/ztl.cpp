#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mqtt/async_client.h>
#include "json.hpp"

using json = nlohmann::json;
using namespace std;
using namespace std::chrono;

const string TEST_BROKER_ADDRESS = "tcp://broker.emqx.io:1883";
const string TEST_CLIENT_ID = "mqtt_test_client_" + to_string(time(nullptr));
const string TEST_TOPIC = "1/device/carControl";
const string RESPONSE_TOPIC = "1/device/carControl/result";

class MqttTestCallback : public virtual mqtt::callback {
public:
    MqttTestCallback() : message_count_(0) {}

    void connection_lost(const string& cause) override {
        cout << "Connection lost: " << cause << endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        message_count_++;
        cout << "Received message " << message_count_ << ": " << msg->to_string() << endl;
        
        try {
            auto response = json::parse(msg->to_string());
            all_messages_.push_back(response);
            
            // 检查消息类型
            if (response.size() == 1 && response.contains("success")) {
                cout << "Processing acknowledgment message" << endl;
                acknowledgment_messages_.push_back(response);
            } 
            // 检查是否为详细结果消息
            else if (response.contains("carcontrol_id") && response.contains("motor1") && response.contains("motor2") && 
                     response.contains("status") && response.contains("status_byte") && response.contains("success")) {
                cout << "Processing detailed result message" << endl;
                result_messages_.push_back(response);
                
                // 解析并记录详细结果
                cout << "Detailed result: " << endl;
                cout << "  carcontrol_id: " << response["carcontrol_id"] << endl;
                cout << "  motor1: " << response["motor1"] << endl;
                cout << "  motor2: " << response["motor2"] << endl;
                cout << "  status: " << response["status"] << endl;
                cout << "  status_byte: " << response["status_byte"] << endl;
                cout << "  success: " << response["success"] << endl;
                if (response.contains("message")) {
                    cout << "  message: " << response["message"] << endl;
                }
            }
            // 检查是否为命令切换反馈消息
            else if (response.contains("message") && response["message"].is_string() && 
                     response["message"].get<string>().find("Executing new command") != string::npos) {
                cout << "Processing command switch feedback message" << endl;
                switch_messages_.push_back(response);
            }
            // 未知消息格式
            else {
                cout << "Received unknown message format" << endl;
            }
        } catch (const exception& e) {
            cout << "Error parsing response: " << e.what() << endl;
        }
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

    bool wait_for_response(int timeout_ms = 5000) {
        auto start = high_resolution_clock::now();
        int prev_message_count = -1;
        
        // 等待至少有一条结果消息，或者超时
        while (result_messages_.empty() && message_count_ < prev_message_count + 5) {
            prev_message_count = message_count_;
            auto now = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(now - start).count();
            if (duration > timeout_ms) {
                cout << "Timeout waiting for responses" << endl;
                cout << "  Total messages received: " << message_count_ << endl;
                cout << "  Acknowledgment messages: " << acknowledgment_messages_.size() << endl;
                cout << "  Result messages: " << result_messages_.size() << endl;
                cout << "  Switch messages: " << switch_messages_.size() << endl;
                return false;
            }
            this_thread::sleep_for(milliseconds(100));
        }
        return true;
    }

    // 获取所有收到的消息
    const vector<json>& get_all_messages() const { return all_messages_; }
    
    // 获取确认消息列表
    const vector<json>& get_acknowledgment_messages() const { return acknowledgment_messages_; }
    
    // 获取结果消息列表
    const vector<json>& get_result_messages() const { return result_messages_; }
    
    // 获取命令切换消息列表
    const vector<json>& get_switch_messages() const { return switch_messages_; }
    
    // 获取最后一条结果消息的success值
    bool is_last_result_success() const {
        if (result_messages_.empty()) {
            return false;
        }
        return result_messages_.back()["success"];
    }
    
    // 获取消息总数
    int get_message_count() const { return message_count_; }
    
    void reset() {
        message_count_ = 0;
        all_messages_.clear();
        acknowledgment_messages_.clear();
        result_messages_.clear();
        switch_messages_.clear();
    }

private:
    int message_count_;                 // 收到的消息总数
    vector<json> all_messages_;         // 所有收到的消息
    vector<json> acknowledgment_messages_; // 确认消息列表
    vector<json> result_messages_;      // 结果消息列表
    vector<json> switch_messages_;      // 命令切换消息列表
};

struct TestCase {
    string name;
    json payload;
    bool expected_success;
    string description;
};

class MqttTest {
public:
    MqttTest() : client_(TEST_BROKER_ADDRESS, TEST_CLIENT_ID), callback_() {
        client_.set_callback(callback_);
    }

    bool connect() {
        try {
            cout << "Connecting to MQTT broker: " << TEST_BROKER_ADDRESS << endl;
            client_.connect()->wait();
            client_.subscribe(RESPONSE_TOPIC, 1);
            cout << "Connected and subscribed to response topic" << endl;
            return true;
        } catch (const mqtt::exception& e) {
            cout << "Connection failed: " << e.what() << endl;
            return false;
        }
    }

    void disconnect() {
        try {
            client_.disconnect()->wait();
            cout << "Disconnected from MQTT broker" << endl;
        } catch (const mqtt::exception& e) {
            cout << "Disconnect failed: " << e.what() << endl;
        }
    }

    bool run_test(const TestCase& test_case) {
        cout << "\n====================================================" << endl;
        cout << "Running test: " << test_case.name << endl;
        cout << "Description: " << test_case.description << endl;
        cout << "Payload: " << test_case.payload.dump() << endl;

        callback_.reset();

        try {
            auto msg = mqtt::make_message(TEST_TOPIC, test_case.payload.dump(), 1, false);
            client_.publish(msg)->wait();
            cout << "Message published" << endl;
        } catch (const mqtt::exception& e) {
            cout << "Publish failed: " << e.what() << endl;
            return false;
        }

        if (!callback_.wait_for_response()) {
            cout << "Test result: FAIL" << endl;
            cout << "Reason: Timeout waiting for response messages" << endl;
            return false;
        }

        // 验证结果消息：检查最后一条结果消息的success字段是否与预期一致
        bool last_msg_success = callback_.is_last_result_success();
        bool test_passed = (last_msg_success == test_case.expected_success);
        
        cout << "Test result: " << (test_passed ? "PASS" : "FAIL") << endl;
        cout << "Expected success: " << test_case.expected_success << ", Actual success: " << last_msg_success << endl;

        if (!test_passed) {
            cout << "Error: Result message validation failed" << endl;
            if (!callback_.get_result_messages().empty()) {
                auto response = callback_.get_result_messages().back();
                if (response.contains("message")) {
                    cout << "Error message: " << response["message"] << endl;
                }
            }
        }

        return test_passed;
    }

    void run_all_tests() {
        vector<TestCase> test_cases = create_test_cases();
        vector<bool> results;

        cout << "Running " << test_cases.size() << " test cases" << endl;

        for (const auto& test_case : test_cases) {
            bool result = run_test(test_case);
            results.push_back(result);
            this_thread::sleep_for(seconds(1)); // Wait a bit between tests
        }

        generate_report(test_cases, results);
    }
    
    // 测试命令覆盖功能：快速连续发送两条命令
    bool run_command_override_test() {
        cout << "\n====================================================" << endl;
        cout << "Running test: Command Override Test" << endl;
        cout << "Description: Testing command override functionality by sending two commands in quick succession" << endl;
        
        // 准备两条测试命令
        json cmd1 = {{"carcontrol_id", "carcontrol001"}, {"motor1", 100}, {"motor2", 100}}; 
        json cmd2 ={{"carcontrol_id", "carcontrol001"}, {"motor1", 200}, {"motor2", 200}}; 
        
        cout << "Command 1: " << cmd1.dump() << endl;
        cout << "Command 2: " << cmd2.dump() << endl;
        
        // 重置回调状态
        callback_.reset();
        
        try {
            // 发送第一条命令
            auto msg1 = mqtt::make_message(TEST_TOPIC, cmd1.dump(), 1, false);
            client_.publish(msg1)->wait();
            cout << "Command 1 published" << endl;
            
            // 短暂延迟（确保第一条命令开始执行但未完成）
            this_thread::sleep_for(milliseconds(100));
            
            // 发送第二条命令
            auto msg2 = mqtt::make_message(TEST_TOPIC, cmd2.dump(), 1, false);
            client_.publish(msg2)->wait();
            cout << "Command 2 published" << endl;
            
            // 等待响应
            if (!callback_.wait_for_response(8000)) { // 延长超时时间，因为要处理多条消息
                cout << "Test result: FAIL" << endl;
                cout << "Reason: Timeout waiting for responses" << endl;
                return false;
            }
            
            // 验证命令覆盖功能
            cout << "\nCommand Override Test Results:" << endl;
            cout << "====================================================" << endl;
            cout << "Total messages received: " << callback_.get_message_count() << endl;
            cout << "Acknowledgment messages: " << callback_.get_acknowledgment_messages().size() << endl;
            cout << "Result messages: " << callback_.get_result_messages().size() << endl;
            cout << "Command switch messages: " << callback_.get_switch_messages().size() << endl;
            
            // 检查是否收到了命令切换消息
            bool has_switch_message = !callback_.get_switch_messages().empty();
            cout << "Has command switch feedback: " << (has_switch_message ? "YES" : "NO") << endl;
            
            // 检查最后一条结果消息的motor值是否与第二条命令一致
            bool motor_values_match = false;
            if (!callback_.get_result_messages().empty()) {
                auto last_result = callback_.get_result_messages().back();
                int actual_motor1 = last_result["motor1"];
                int actual_motor2 = last_result["motor2"];
                cout << "Last result motor1: " << actual_motor1 << " (expected: " << cmd2["motor1"] << ")" << endl;
                cout << "Last result motor2: " << actual_motor2 << " (expected: " << cmd2["motor2"] << ")" << endl;
                motor_values_match = (actual_motor1 == cmd2["motor1"]) && (actual_motor2 == cmd2["motor2"]);
            }
            
            // 验证第二条命令是否成功执行
            bool last_result_success = callback_.is_last_result_success();
            cout << "Last result success: " << (last_result_success ? "YES" : "NO") << endl;
            
            // 综合判断测试结果
            bool test_passed = has_switch_message && motor_values_match && last_result_success;
            
            cout << "\nTest result: " << (test_passed ? "PASS" : "FAIL") << endl;
            if (test_passed) {
                cout << "Command override functionality verified successfully!" << endl;
                cout << "- First command was terminated by second command" << endl;
                cout << "- Second command was executed successfully" << endl;
                cout << "- Command switch feedback was received" << endl;
            } else {
                cout << "Command override test failed!" << endl;
                if (!has_switch_message) {
                    cout << "Reason: No command switch feedback message received" << endl;
                }
                if (!motor_values_match) {
                    cout << "Reason: Motor values in last result do not match second command" << endl;
                }
                if (!last_result_success) {
                    cout << "Reason: Last result was not successful" << endl;
                }
            }
            
            cout << "====================================================" << endl;
            return test_passed;
            
        } catch (const mqtt::exception& e) {
            cout << "Test result: FAIL" << endl;
            cout << "Error: " << e.what() << endl;
            return false;
        }
        
        
    }

private:
    vector<TestCase> create_test_cases() {
        vector<TestCase> test_cases;

        // 正常测试用例
        test_cases.push_back({
            "Normal operation with positive values",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", 300}, {"motor2", 300}},
            true,
            "Testing normal operation with positive motor values"
        });

        test_cases.push_back({
            "Normal operation with negative values",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", -200}, {"motor2", -200}},
            true,
            "Testing normal operation with negative motor values"
        });

        test_cases.push_back({
            "Normal operation with mixed values",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", 100}, {"motor2", -100}},
            true,
            "Testing normal operation with mixed positive and negative motor values"
        });

        // 边界值测试用例
        test_cases.push_back({
            "Motor values at maximum positive",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", 1500}, {"motor2", 1500}},
            true,
            "Testing motor values at maximum positive limit"
        });

        test_cases.push_back({
            "Motor values at maximum negative",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", -1500}, {"motor2", -1500}},
            true,
            "Testing motor values at maximum negative limit"
        });

        test_cases.push_back({
            "Motor values at zero",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", 0}, {"motor2", 0}},
            true,
            "Testing motor values at zero"
        });

        // 异常测试用例
        test_cases.push_back({
            "Missing carcontrol_id",
            {{"motor1", 100}, {"motor2", 100}},
            false,
            "Testing missing required carcontrol_id field"
        });

        test_cases.push_back({
            "Missing both motor fields",
            {{"carcontrol_id", "carcontrol001"}},
            false,
            "Testing missing both motor1 and motor2 fields"
        });

        test_cases.push_back({
            "Missing motor1",
            {{"carcontrol_id", "carcontrol001"}, {"motor2", 100}},
            false,
            "Testing missing motor1 field"
        });

        test_cases.push_back({
            "Missing motor2",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", 100}},
            false,
            "Testing missing motor2 field"
        });

        test_cases.push_back({
            "Motor1 value exceeds maximum positive",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", 1600}, {"motor2", 300}},
            false,
            "Testing motor1 value exceeding maximum positive limit"
        });

        test_cases.push_back({
            "Motor2 value exceeds maximum negative",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", 300}, {"motor2", -1600}},
            false,
            "Testing motor2 value exceeding maximum negative limit"
        });

        test_cases.push_back({
            "Invalid carcontrol_id",
            {{"carcontrol_id", "invalid_id"}, {"motor1", 300}, {"motor2", 300}},
            false,
            "Testing invalid carcontrol_id"
        });

        test_cases.push_back({
            "Empty carcontrol_id",
            {{"carcontrol_id", ""}, {"motor1", 300}, {"motor2", 300}},
            false,
            "Testing empty carcontrol_id"
        });

        test_cases.push_back({
            "Non-numeric motor values",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", "abc"}, {"motor2", 300}},
            false,
            "Testing non-numeric motor1 value"
        });

        test_cases.push_back({
            "Additional fields",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", 300}, {"motor2", 300}, {"extra_field", "value"}},
            true,
            "Testing with additional non-required fields"
        });

        // 新增测试用例：验证两条消息格式的处理
        test_cases.push_back({
            "Two-message response format",
            {{"carcontrol_id", "carcontrol001"}, {"motor1", 100}, {"motor2", 200}},
            true,
            "Testing two-message response format: acknowledgment + detailed result"
        });

        return test_cases;
    }

    void generate_report(const vector<TestCase>& test_cases, const vector<bool>& results) {
        cout << "\n====================================================" << endl;
        cout << "TEST REPORT" << endl;
        cout << "====================================================" << endl;

        int passed = 0;
        int failed = 0;

        for (size_t i = 0; i < test_cases.size(); ++i) {
            cout << test_cases[i].name << ": " << (results[i] ? "PASS" : "FAIL") << endl;
            if (results[i]) {
                passed++;
            } else {
                failed++;
            }
        }

        cout << "====================================================" << endl;
        cout << "Summary: " << passed << " passed, " << failed << " failed out of " << test_cases.size() << " tests" << endl;
        cout << "Pass rate: " << (static_cast<double>(passed) / test_cases.size() * 100) << "%" << endl;
        cout << "====================================================" << endl;
    }

    mqtt::async_client client_;
    MqttTestCallback callback_;
};

int main() {
    MqttTest test;

    if (!test.connect()) {
        cout << "Failed to connect to MQTT broker. Exiting." << endl;
        return 1;
    }

    // 运行所有常规测试用例
    //test.run_all_tests();
    
    // 运行命令覆盖测试
    bool override_test_result = test.run_command_override_test();
    cout << "Command override test result: " << (override_test_result ? "PASS" : "FAIL") << endl;
    
    test.disconnect();

    return 0;
}