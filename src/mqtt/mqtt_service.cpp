#include "mqtt_service.h"
#include <iostream>

MqttService::MqttService(const std::string& serverURI,
                         const std::string& clientId,
                         JobScheduler& scheduler)
    : client_(serverURI, clientId),
      dispatcher_(scheduler)
{
    client_.set_callback(*this);
}

void MqttService::start()
{
    client_.connect()->wait();

    client_.subscribe("device/camera/getRealImage", 1);
    client_.subscribe("device/plc/operate", 1);
    client_.subscribe("device/config/update", 1);

    std::cout << "MQTT connected & subscribed." << std::endl;
}

void MqttService::connection_lost(const std::string& cause)
{
    std::cout << "[MQTT] Connection lost: " << cause << std::endl;

    // 自动重连
    while (true)
    {
        try {
            std::cout << "[MQTT] Reconnecting..." << std::endl;
            client_.reconnect()->wait();
            std::cout << "[MQTT] Reconnected!" << std::endl;

            client_.subscribe("device/camera/getRealImage", 1);
            client_.subscribe("device/plc/operate", 1);
            client_.subscribe("device/config/update", 1);
            return;
        }
        catch (...) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
}

void MqttService::message_arrived(mqtt::const_message_ptr msg)
{
    dispatcher_.onMqttMessage(msg->get_topic(), msg->to_string());
}

void MqttService::publish(const std::string& topic,
                          const std::string& payload,
                          int qos,
                          bool retained)
{
    try {
        auto msg = mqtt::make_message(topic, payload, qos, retained);
        client_.publish(msg);
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Publish failed: " << e.what() << std::endl;
    }
}
