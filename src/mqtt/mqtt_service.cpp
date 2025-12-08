#include "mqtt_service.h"
#include <iostream>

MqttService::MqttService(const std::string& serverURI,
                         const std::string& clientId,
                         JobScheduler& scheduler,
                         const std::string& boxId,
                         ICommandDispatcher* dispatcher)
    : client_(serverURI, clientId),
    boxId_(boxId),
    dispatcher_(dispatcher)
{
    client_.set_callback(*this);
}

void MqttService::start()
{
    try{
        client_.connect()->wait();

    client_.subscribe("box" + boxId_ + "/device/control/solenoid/direct", 1);
    client_.subscribe("box" + boxId_ + "/device/control/solenoid/verified", 1);
    client_.subscribe("box" + boxId_ + "/device/sensor/custom", 1);
    client_.subscribe("box" + boxId_ + "/device/sensor/gpio", 1);
    client_.subscribe("box" + boxId_ + "/device/sensor/modbus", 1);
    client_.subscribe("box" + boxId_ + "/device/control/plc/direct", 1);
    client_.subscribe("box" + boxId_ + "/device/camera", 1);

    std::cout << "MQTT connected & subscribed." << std::endl;
    }catch(const mqtt::exception& e){
        std::cerr << "[MQTT] Connect failed: " << e.what() << std::endl;
        connection_lost("initial connect failed");
    }

}

void MqttService::connection_lost(const std::string& cause)
{
    std::cout << "[MQTT] Connection lost: " << cause << std::endl;
    int retryCount = 0;
    const int maxRetries = 5; // 最大重试10次
    while (retryCount < maxRetries)
    {
        try {
            std::cout << "[MQTT] Reconnecting... (retry " << retryCount + 1 << "/" << maxRetries << ")" << std::endl;

            client_.reconnect()->wait();
            std::cout << "[MQTT] Reconnected!" << std::endl;

            client_.subscribe("box" + boxId_ + "/device/control/solenoid/direct", 1);
            client_.subscribe("box" + boxId_ + "/device/control/solenoid/verified", 1);
            client_.subscribe("box" + boxId_ + "/device/sensor/custom", 1);
            client_.subscribe("box" + boxId_ + "/device/sensor/gpio", 1);
            client_.subscribe("box" + boxId_ + "/device/sensor/modbus", 1);
            client_.subscribe("box" + boxId_ + "/device/control/plc/direct", 1);
            client_.subscribe("box" + boxId_ + "/device/camera", 1);
            return;
        }
        catch (const mqtt::exception& e) {
            std::cerr << "[MQTT] Reconnect failed: " << e.what() << std::endl;
            retryCount++;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    std::cerr << "[MQTT] Max retries reached, exit reconnect loop" << std::endl;
}

void MqttService::message_arrived(mqtt::const_message_ptr msg)
{
    if (dispatcher_) {
        dispatcher_->onMessage(msg->get_topic(), msg->to_string());
    }
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
