#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;
using namespace std::chrono;

const string TEST_BROKER_ADDRESS = "tcp://broker.emqx.io:1883";
const string TEST_CLIENT_ID = "mqtt_test_client_" + to_string(time(nullptr));
const string TEST_TOPIC = "device/control/carcontrol";
const string RESPONSE_TOPIC = "device/carcontrol/result";

class MqttTestCallback : public virtual mqtt::callback {
public:
    MqttTestCallback() : received_(false), success_(false) {}

    void connection_lost(const string& cause) override {
        cout << "Connection lost: " << cause << endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        cout << "Received response: " << msg->to_string() << endl;
        
        try {
            auto response = json::parse(msg->to_string());
            response_ = response;
            success_ = response["success"];
            received_ = true;
            
            if (response.contains("status_description")) {
                cout << "Status description: " << response["status_description"] << endl;
            }
        } catch (const exception& e) {
            cout << "Error parsing response: " << e.what() << endl;
        }
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

    bool wait_for_response(int timeout_ms = 5000) {
        auto start = high_resolution_clock::now();
        while (!received_) {
            auto now = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(now - start).count();
            if (duration > timeout_ms) {
                cout << "Timeout waiting for response" << endl;
                return false;
            }
            this_thread::sleep_for(milliseconds(100));
        }
        return true;
    }

    json get_response() const { return response_; }
    bool is_success() const { return success_; }
    void reset() { received_ = false; success_ = false; response_.clear(); }

private:
    bool received_;
    bool success_;
    json response_;
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
            return false;
        }

        bool actual_success = callback_.is_success();
        bool test_passed = (actual_success == test_case.expected_success);

        cout << "Test result: " << (test_passed ? "PASS" : "FAIL") << endl;
        cout << "Expected success: " << test_case.expected_success << ", Actual success: " << actual_success << endl;

        if (!test_passed) {
            auto response = callback_.get_response();
            if (response.contains("message")) {
                cout << "Error message: " << response["message"] << endl;
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

    test.run_all_tests();
    test.disconnect();

    return 0;
}