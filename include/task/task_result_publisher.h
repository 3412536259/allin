#ifndef TASK_RESULT_PUBLISHER_H
#define TASK_RESULT_PUBLISHER_H

#include "itask_result_publisher.h"
#include "mqtt_service.h"
#include <curl/curl.h>
class MqttPublisher : public ITaskResultPublisher
{
public:
    MqttPublisher(MqttService* mqtt) : mqtt_(mqtt) {mqtt_->start();}
    void publish(const std::string& topic,
                 const std::string& message) override {
        mqtt_->publish(topic, message);
    }
private:
    MqttService* mqtt_;
};

class HttpPublisher : public ITaskResultPublisher {
public:
    // 构造时指定你的 HTTP 服务器地址，例如：http://127.0.0.1:8080/report
    explicit HttpPublisher(const std::string& url)
        : url_(url) 
    {
        curl_global_init(CURL_GLOBAL_ALL);
    }

    ~HttpPublisher() {
        curl_global_cleanup();
    }

    // 将 topic 和 message 一起 POST 到你的 http server
    void publish(const std::string& topic,
                 const std::string& message) override 
    {
        CURL* curl = curl_easy_init();
        if (!curl) return;

        // JSON 格式发送（常见形式）
        std::string json = R"({"topic":")" + topic + R"(","message":")" + message + R"("})";

        curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json.size());

        // 设置 Content-Type: application/json
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 执行 POST
        CURLcode res = curl_easy_perform(curl);

        // 清理资源
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

private:
    std::string url_;
};
// class HttpPublisher : public ITaskResultPublisher {
// public:
//     void publish(const std::string& topic,
//                  const std::string& message) override {
//        std::lock_guard<std::mutex> lk(mu_);
//        lastTopic_ = topic;
//        lastMessage_ = message;
//        ready_ = true;
//        cv_.notify_one();
//     }

//     // wait for a published message (returns empty string if timeout)
//     std::string waitForMessage(int timeoutMs = 1000) {
//         std::unique_lock<std::mutex> lk(mu_);
//         if (!ready_) {
//             cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs));
//         }
//         ready_ = false;
//         return lastMessage_;
//     }

//     std::string lastTopic() const { return lastTopic_; }

// private:
//     mutable std::mutex mu_;
//     std::condition_variable cv_;
//     bool ready_ = false;
//     std::string lastTopic_;
//     std::string lastMessage_;
// };

#endif