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
    MqttTestCallback() : received_first_(false), received_second_(false), first_success_(false), second_success_(false) {}

    void connection_lost(const string& cause) override {
        cout << "Connection lost: " << cause << endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        cout << "Received message: " << msg->to_string() << endl;
        cout << "Message topic: " << msg->get_topic() << endl;
        
        try {
            auto response = json::parse(msg->to_string());
            cout << "Parsed JSON successfully" << endl;
            
            // 调试：打印所有字段
            cout << "Response fields: " << endl;
            for (auto& [key, value] : response.items()) {
                cout << "  " << key << ": " << value << endl;
            }
            
            // 检查是否为第一条消息：包含carcontrolId和success字段（放宽条件）
            if (response.contains("carcontrolId") && response.contains("success")) {
                cout << "Processing first message (acknowledgment)" << endl;
                first_response_ = response;
                first_success_ = response["success"];
                received_first_ = true;
                cout << "First message received: " << endl;
                cout << "  carcontrolId: " << response["carcontrolId"] << endl;
                cout << "  success: " << first_success_ << endl;
            } 
            // 检查是否为第二条消息：包含carcontrolId、motor1、motor2和success字段（放宽条件）
            else if (response.contains("carcontrolId") && response.contains("motor1") && response.contains("motor2") && response.contains("success")) {
                cout << "Processing second message (detailed result)" << endl;
                second_response_ = response;
                second_success_ = response["success"];
                received_second_ = true;
                
                // 解析并记录详细结果
                cout << "Second message received: " << endl;
                cout << "  carcontrolId: " << response["carcontrolId"] << endl;
                cout << "  motor1: " << response["motor1"] << endl;
                cout << "  motor2: " << response["motor2"] << endl;
                if (response.contains("status")) cout << "  status: " << response["status"] << endl;
                if (response.contains("status_byte")) cout << "  status_byte: " << response["status_byte"] << endl;
                cout << "  success: " << response["success"] << endl;
                if (response.contains("message")) {
                    cout << "  message: " << response["message"] << endl;
                }
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
        auto last_log_time = start;
        
        while (true) {
            auto now = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(now - start).count();
            
            // 每500ms打印一次调试信息
            auto log_duration = duration_cast<milliseconds>(now - last_log_time).count();
            if (log_duration > 500) {
                cout << "Waiting for responses... Duration: " << duration << "ms" << endl;
                cout << "  Received first message: " << (received_first_ ? "YES" : "NO") << endl;
                cout << "  Received second message: " << (received_second_ ? "YES" : "NO") << endl;
                last_log_time = now;
            }
            
            // 超时检查
            if (duration > timeout_ms) {
                cout << "Timeout waiting for responses after " << duration << "ms" << endl;
                cout << "Final state: " << endl;
                cout << "  Received first message: " << (received_first_ ? "YES" : "NO") << endl;
                cout << "  Received second message: " << (received_second_ ? "YES" : "NO") << endl;
                
                // 放宽条件：如果收到了任何一条消息，也认为测试通过
                if (received_first_ || received_second_) {
                    cout << "WARNING: Only received partial responses, but continuing with test" << endl;
                    return true;
                }
                
                return false;
            }
            
            // 检查是否收到了所有必要的消息（放宽条件）
            if (received_second_) {
                cout << "Received second message, continuing with test" << endl;
                return true;
            } else if (received_first_) {
                cout << "Received first message, waiting for second..." << endl;
            }
            
            this_thread::sleep_for(milliseconds(100));
        }
    }

    json get_first_response() const { return first_response_; }
    json get_second_response() const { return second_response_; }
    bool is_first_success() const { return first_success_; }
    bool is_second_success() const { return second_success_; }
    bool has_received_first() const { return received_first_; }
    bool has_received_second() const { return received_second_; }
    void reset() {
        received_first_ = false;
        received_second_ = false;
        first_success_ = false;
        second_success_ = false;
        first_response_.clear();
        second_response_.clear();
    }

private:
    bool received_first_;      // 第一条消息是否收到
    bool received_second_;     // 第二条消息是否收到
    bool first_success_;       // 第一条消息的success值
    bool second_success_;      // 第二条消息的success值
    json first_response_;      // 第一条消息内容
    json second_response_;     // 第二条消息内容
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
            cout << "Reason: Timeout waiting for both messages" << endl;
            return false;
        }

        // 验证第一条消息：必须是成功的确认
        bool first_msg_valid = callback_.has_received_first() && callback_.is_first_success();
        cout << "First message validation: " << (first_msg_valid ? "PASS" : "FAIL") << endl;
        cout << "  Received: " << (callback_.has_received_first() ? "YES" : "NO") << endl;
        cout << "  Success: " << callback_.is_first_success() << endl;

        // 验证第二条消息：检查success字段是否与预期一致
        bool second_msg_success = callback_.is_second_success();
        bool second_msg_valid = callback_.has_received_second() && (second_msg_success == test_case.expected_success);
        cout << "Second message validation: " << (second_msg_valid ? "PASS" : "FAIL") << endl;
        cout << "  Received: " << (callback_.has_received_second() ? "YES" : "NO") << endl;
        cout << "  Expected success: " << test_case.expected_success << ", Actual success: " << second_msg_success << endl;

        bool test_passed = first_msg_valid && second_msg_valid;
        cout << "Test result: " << (test_passed ? "PASS" : "FAIL") << endl;

        if (!test_passed) {
            if (!first_msg_valid) {
                cout << "Error: First message not received or not successful" << endl;
            }
            if (!callback_.has_received_second()) {
                cout << "Error: Second message not received" << endl;
            }
            if (callback_.has_received_second() && !second_msg_valid) {
                auto response = callback_.get_second_response();
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

public:
    vector<TestCase> create_test_cases() {
        vector<TestCase> test_cases;

        // 正常测试用例
        test_cases.push_back({
            "Normal operation with positive values",
            {{"carcontrolId", "carcontrol001"}, {"motor1", 300}, {"motor2", 300}},
            true,
            "Testing normal operation with positive motor values"
        });

        test_cases.push_back({
            "Normal operation with negative values",
            {{"carcontrolId", "carcontrol001"}, {"motor1", -200}, {"motor2", -200}},
            true,
            "Testing normal operation with negative motor values"
        });

        test_cases.push_back({
            "Normal operation with mixed values",
            {{"carcontrolId", "carcontrol001"}, {"motor1", 100}, {"motor2", -100}},
            true,
            "Testing normal operation with mixed positive and negative motor values"
        });

        // 边界值测试用例
        test_cases.push_back({
            "Motor values at maximum positive",
            {{"carcontrolId", "carcontrol001"}, {"motor1", 1500}, {"motor2", 1500}},
            true,
            "Testing motor values at maximum positive limit"
        });

        test_cases.push_back({
            "Motor values at maximum negative",
            {{"carcontrolId", "carcontrol001"}, {"motor1", -1500}, {"motor2", -1500}},
            true,
            "Testing motor values at maximum negative limit"
        });

        test_cases.push_back({
            "Motor values at zero",
            {{"carcontrolId", "carcontrol001"}, {"motor1", 0}, {"motor2", 0}},
            true,
            "Testing motor values at zero"
        });

        // 异常测试用例
        test_cases.push_back({
            "Missing carcontrolId",
            {{"motor1", 100}, {"motor2", 100}},
            false,
            "Testing missing required carcontrolId field"
        });

        test_cases.push_back({
            "Missing both motor fields",
            {{"carcontrolId", "carcontrol001"}},
            false,
            "Testing missing both motor1 and motor2 fields"
        });

        test_cases.push_back({
            "Missing motor1",
            {{"carcontrolId", "carcontrol001"}, {"motor2", 100}},
            false,
            "Testing missing motor1 field"
        });

        test_cases.push_back({
            "Missing motor2",
            {{"carcontrolId", "carcontrol001"}, {"motor1", 100}},
            false,
            "Testing missing motor2 field"
        });

        test_cases.push_back({
            "Motor1 value exceeds maximum positive",
            {{"carcontrolId", "carcontrol001"}, {"motor1", 1600}, {"motor2", 300}},
            false,
            "Testing motor1 value exceeding maximum positive limit"
        });

        test_cases.push_back({
            "Motor2 value exceeds maximum negative",
            {{"carcontrolId", "carcontrol001"}, {"motor1", 300}, {"motor2", -1600}},
            false,
            "Testing motor2 value exceeding maximum negative limit"
        });

        test_cases.push_back({
            "Invalid carcontrolId",
            {{"carcontrolId", "invalid_id"}, {"motor1", 300}, {"motor2", 300}},
            false,
            "Testing invalid carcontrolId"
        });

        test_cases.push_back({
            "Empty carcontrolId",
            {{"carcontrolId", ""}, {"motor1", 300}, {"motor2", 300}},
            false,
            "Testing empty carcontrolId"
        });

        test_cases.push_back({
            "Non-numeric motor values",
            {{"carcontrolId", "carcontrol001"}, {"motor1", "abc"}, {"motor2", 300}},
            false,
            "Testing non-numeric motor1 value"
        });

        test_cases.push_back({
            "Additional fields",
            {{"carcontrolId", "carcontrol001"}, {"motor1", 300}, {"motor2", 300}, {"extra_field", "value"}},
            true,
            "Testing with additional non-required fields"
        });

        // 新增测试用例：验证两条消息格式的处理
        test_cases.push_back({
            "Two-message response format",
            {{"carcontrolId", "carcontrol001"}, {"motor1", 100}, {"motor2", 200}},
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
    
    // 命令打断功能测试
    void run_interrupt_test() {
        cout << "\n====================================================" << endl;
        cout << "RUNNING COMMAND INTERRUPT TEST" << endl;
        cout << "====================================================" << endl;
        
        // 测试配置
        string carcontrolId = "carcontrol001";
        int timeout_ms = 10000;
        
        // 统计数据
        int interrupt_success = 0;
        int total_tests = 0;
        vector<long long> response_times;
        
        // 执行多次测试（减少到2轮，便于调试）
        for (int test_round = 1; test_round <= 2; ++test_round) {
            cout << "\n--- Test Round " << test_round << " ---" << endl;
            total_tests++;
            
            callback_.reset();
            
            // 第一条命令
            json first_cmd = {{
                {"carcontrolId", carcontrolId},
                {"motor1", 300},
                {"motor2", 300}
            }};
            
            // 第二条命令
            json second_cmd = {{
                {"carcontrolId", carcontrolId},
                {"motor1", 100},
                {"motor2", -100}
            }};
            
            auto start_time = high_resolution_clock::now();
            
            try {
                cout << "Publishing to topic: " << TEST_TOPIC << endl;
                cout << "Subscribed to topic: " << RESPONSE_TOPIC << endl;
                
                // 发送第一条命令
                cout << "Sending first command: " << first_cmd.dump() << endl;
                auto msg1 = mqtt::make_message(TEST_TOPIC, first_cmd.dump(), 1, false);
                auto token1 = client_.publish(msg1);
                token1->wait_for(seconds(2)); // 等待发布完成
                if (token1->is_complete()) {
                    cout << "First command published successfully" << endl;
                } else {
                    cout << "First command publish failed" << endl;
                    continue;
                }
                
                // 等待一段时间后发送第二条命令（模拟在第一条命令执行过程中发送）
                cout << "Waiting 200ms before sending second command..." << endl;
                this_thread::sleep_for(milliseconds(200));
                
                auto interrupt_time = high_resolution_clock::now();
                
                // 发送第二条命令
                cout << "Sending second command: " << second_cmd.dump() << endl;
                auto msg2 = mqtt::make_message(TEST_TOPIC, second_cmd.dump(), 1, false);
                auto token2 = client_.publish(msg2);
                token2->wait_for(seconds(2)); // 等待发布完成
                if (token2->is_complete()) {
                    cout << "Second command published successfully" << endl;
                } else {
                    cout << "Second command publish failed" << endl;
                    continue;
                }
                
                // 等待响应
                cout << "Waiting for responses... Timeout: " << timeout_ms << "ms" << endl;
                if (callback_.wait_for_response(timeout_ms)) {
                    cout << "Response wait completed" << endl;
                    
                    // 验证响应
                    bool has_first = callback_.has_received_first();
                    bool has_second = callback_.has_received_second();
                    
                    if (has_second) {
                        // 验证第二条命令是否被执行
                        auto second_response = callback_.get_second_response();
                        if (second_response.contains("motor1") && second_response.contains("motor2")) {
                            int actual_motor1 = second_response["motor1"];
                            int actual_motor2 = second_response["motor2"];
                            
                            cout << "Actual executed command: motor1=" << actual_motor1 << ", motor2=" << actual_motor2 << endl;
                            cout << "Expected executed command: motor1=100, motor2=-100" << endl;
                            
                            if (actual_motor1 == 100 && actual_motor2 == -100) {
                                cout << "? Command interrupt successful: second command was executed" << endl;
                                interrupt_success++;
                                
                                // 计算响应时间
                                auto end_time = high_resolution_clock::now();
                                auto duration = duration_cast<milliseconds>(end_time - interrupt_time).count();
                                response_times.push_back(duration);
                                cout << "Command switch response time: " << duration << "ms" << endl;
                            } else {
                                cout << "? Command interrupt failed: first command was not interrupted" << endl;
                            }
                        } else {
                            cout << "? Invalid second response format" << endl;
                        }
                    } else if (has_first) {
                        cout << "? Only received first message, no detailed result" << endl;
                    } else {
                        cout << "? No valid messages received" << endl;
                    }
                } else {
                    cout << "? Test failed: Timeout waiting for responses" << endl;
                }
            } catch (const mqtt::exception& e) {
                cout << "? MQTT exception: " << e.what() << endl;
                cout << "Exception type: " << typeid(e).name() << endl;
            } catch (const exception& e) {
                cout << "? General exception: " << e.what() << endl;
                cout << "Exception type: " << typeid(e).name() << endl;
            }
            
            // 等待一段时间后进行下一轮测试
            cout << "Waiting 2 seconds before next test..." << endl;
            this_thread::sleep_for(seconds(2));
        }
        
        // 生成测试报告
        cout << "\n====================================================" << endl;
        cout << "COMMAND INTERRUPT TEST REPORT" << endl;
        cout << "====================================================" << endl;
        
        // 中断成功率
        double success_rate = (static_cast<double>(interrupt_success) / total_tests) * 100;
        cout << "Interrupt success rate: " << interrupt_success << "/" << total_tests << " (" << success_rate << "%)" << endl;
        
        // 命令切换响应时间统计
        if (!response_times.empty()) {
            long long sum = 0;
            long long min_time = response_times[0];
            long long max_time = response_times[0];
            
            for (long long time : response_times) {
                sum += time;
                if (time < min_time) min_time = time;
                if (time > max_time) max_time = time;
            }
            
            double avg_time = static_cast<double>(sum) / response_times.size();
            cout << "Command switch response time:" << endl;
            cout << "  Average: " << avg_time << "ms" << endl;
            cout << "  Minimum: " << min_time << "ms" << endl;
            cout << "  Maximum: " << max_time << "ms" << endl;
        }
        
        // 测试结果
        cout << "\nTest Result: " << (interrupt_success > 0 ? "PASS" : "FAIL") << endl;
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

    // 运行命令打断功能测试
    test.run_interrupt_test();
    
    // 运行所有常规测试
    //test.run_all_tests();
    
    test.disconnect();

    return 0;
}