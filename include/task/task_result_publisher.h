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
       //http实现
    }
};

#endif