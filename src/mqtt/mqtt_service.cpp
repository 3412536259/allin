#include "mqtt_service.h"
#include <iostream>

MqttService::MqttService(const std::string& serverURI,
                         const std::string& clientId,
                         ICommandDispatcher* dispatcher)
    : client_(serverURI, clientId),
      dispatcher_(dispatcher)
{
    client_.set_callback(*this);
}

void MqttService::start()
{
    try{
        client_.connect()->wait();

        client_.subscribe("device/camera/getRealImage", 1);
        client_.subscribe("device/plc/operate", 1);
        client_.subscribe("device/config/update", 1);

        std::cout << "MQTT connected & subscribed." << std::endl;
    }catch(const mqtt::exception& e){
        std::cerr << "[MQTT] Connect failed: " << e.what() << std::endl;
        //初始化重连
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

            // 重新订阅主题
            client_.subscribe("device/camera/getRealImage", 1);
            client_.subscribe("device/plc/operate", 1);
            client_.subscribe("device/config/update", 1);
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
