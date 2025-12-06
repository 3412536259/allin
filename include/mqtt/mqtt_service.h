#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <mqtt/async_client.h>
#include "JobScheduler.h"
#include "mqtt_command_dispatcher.h"

class MqttService : public virtual mqtt::callback
{
public:
    MqttService(const std::string& serverURI,
                const std::string& clientId,
                JobScheduler& scheduler);

    void start();

    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr tok) override {}
    void publish(const std::string& topic, const std::string& payload, int qos = 1, bool retained = false);
private:
    mqtt::async_client client_;
    MqttCommandDispatcher dispatcher_;
};

#endif
