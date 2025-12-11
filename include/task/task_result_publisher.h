#ifndef TASK_RESULT_PUBLISHER_H
#define TASK_RESULT_PUBLISHER_H

#include "itask_result_publisher.h"
#include "mqtt_service.h"
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
    void publish(const std::string& topic,
                 const std::string& message) override {
       std::lock_guard<std::mutex> lk(mu_);
       lastTopic_ = topic;
       lastMessage_ = message;
       ready_ = true;
       cv_.notify_one();
    }

    // wait for a published message (returns empty string if timeout)
    std::string waitForMessage(int timeoutMs = 1000) {
        std::unique_lock<std::mutex> lk(mu_);
        if (!ready_) {
            cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs));
        }
        ready_ = false;
        return lastMessage_;
    }

    std::string lastTopic() const { return lastTopic_; }

private:
    mutable std::mutex mu_;
    std::condition_variable cv_;
    bool ready_ = false;
    std::string lastTopic_;
    std::string lastMessage_;
};

#endif