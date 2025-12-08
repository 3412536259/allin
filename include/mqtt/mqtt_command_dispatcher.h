#ifndef MQTT_COMMAND_DISPATCHER_H
#define MQTT_COMMAND_DISPATCHER_H

#include <string>
#include <memory>
#include <json.hpp>
#include "JobScheduler.h"
#include "task.h"

class MqttCommandDispatcher
{
public:
    MqttCommandDispatcher(JobScheduler& scheduler);
    void onMqttMessage(const std::string& topic, const std::string& payload);

private:
    JobScheduler& scheduler_;
    std::string boxId_;
    void handleGetRealImage(const nlohmann::json& j);
    void handleOperatePlc(const nlohmann::json& j);
    void handleUpdateConfig(const nlohmann::json& j);

};


#endif
