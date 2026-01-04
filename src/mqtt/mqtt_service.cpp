#include "mqtt_service.h"
#include "mqtt_topics.h"
#include <iostream>
#include "logger.h"
#include <thread>
#include <chrono>
#include <algorithm>

// const std::string GET_REAL_IMAGE_TOPIC = "device/camera/getRealImage";
// const std::string OPERATE_PLC_TOPIC = "device/plc/operate";
// const std::string UPDATE_CONFIG_TOPIC = "device/config/update";
// const std::string GET_SENSOR_DATA_TOPIC = "device/sensor/status";

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

        client_.subscribe(GET_REAL_IMAGE_TOPIC, 1);
        client_.subscribe(OPERATE_PLC_TOPIC, 1);
        client_.subscribe(UPDATE_CONFIG_TOPIC, 1);
        client_.subscribe(GET_SENSOR_DATA_TOPIC, 1);
        client_.subscribe(OPERATE_CAR_TOPIC, 1);
        client_.subscribe(GET_ALL_DEVICE_STATUS_TOPIC, 1);
        client_.subscribe(OPERATE_PLC_WITH_VERIFY_TOPIC, 1);
        client_.subscribe(UPDATE_CONFIG, 1);

        std::cout << "MQTT connected & subscribed." << std::endl;
        LOG_INFO("MQTT connected & subscribed.");
    }catch(const mqtt::exception& e){
        std::cerr << "[MQTT] Connect failed: " << e.what() << std::endl;
        LOG_ERROR(("MQTT connect failed: " + std::string(e.what())).c_str());
        // 初始化重连（会在后台一直重试）
        connection_lost("initial connect failed");
    }

}

void MqttService::connection_lost(const std::string& cause)
{
    std::cout << "[MQTT] Connection lost: " << cause << std::endl;
    LOG_ERROR(("MQTT connection lost: " + cause).c_str());

    // === 原来的有限重试逻辑（注释保留） ===
    /*
    int retryCount = 0;
    const int maxRetries = 5; // 最大重试10次
    while (retryCount < maxRetries)
    {
        try {
            std::cout << "[MQTT] Reconnecting... (retry " << retryCount + 1 << "/" << maxRetries << ")" << std::endl;
            LOG_INFO(("MQTT reconnecting... (retry " + std::to_string(retryCount + 1) + "/" + std::to_string(maxRetries)).c_str());
            client_.reconnect()->wait();
            std::cout << "[MQTT] Reconnected!" << std::endl;
            LOG_INFO("MQTT reconnected.");

            // 重新订阅主题
            client_.subscribe(GET_REAL_IMAGE_TOPIC, 1);
            client_.subscribe(OPERATE_PLC_TOPIC, 1);
            client_.subscribe(UPDATE_CONFIG_TOPIC, 1);
            client_.subscribe(GET_SENSOR_DATA_TOPIC, 1);
            client_.subscribe(OPERATE_CAR_TOPIC, 1);
            client_.subscribe(GET_ALL_DEVICE_STATUS_TOPIC, 1);
            client_.subscribe(OPERATE_PLC_WITH_VERIFY_TOPIC, 1);
            client_.subscribe(UPDATE_CONFIG, 1);

            return;
        }
        catch (const mqtt::exception& e) {
            std::cerr << "[MQTT] Reconnect failed: " << e.what() << std::endl;
            LOG_ERROR(("MQTT reconnect failed: " + std::string(e.what())).c_str());
            retryCount++;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    std::cerr << "[MQTT] Max retries reached, exit reconnect loop" << std::endl;
    LOG_ERROR("MQTT max retries reached, exit reconnect loop.");
    */
    // === 以上为保留旧逻辑（已注释） ===

    // 如果已有后台重连在运行，直接返回
    if (reconnecting_.exchange(true)) {
        std::cout << "[MQTT] Reconnect already in progress, skipping." << std::endl;
        return;
    }

    // 后台线程：无限重连，带指数退避（上限60秒）
    std::thread([this]() {
        int attempt = 0;
        int sleepSec = 2;
        while (true) {
            try {
                attempt++;
                std::cout << "[MQTT] Reconnecting... (attempt " << attempt << ")" << std::endl;
                LOG_INFO(("MQTT reconnecting... (attempt " + std::to_string(attempt) + ")").c_str());

                client_.reconnect()->wait();

                std::cout << "[MQTT] Reconnected!" << std::endl;
                LOG_INFO("MQTT reconnected.");

                // 重新订阅主题
                client_.subscribe(GET_REAL_IMAGE_TOPIC, 1);
                client_.subscribe(OPERATE_PLC_TOPIC, 1);
                client_.subscribe(UPDATE_CONFIG_TOPIC, 1);
                client_.subscribe(GET_SENSOR_DATA_TOPIC, 1);
                client_.subscribe(OPERATE_CAR_TOPIC, 1);
                client_.subscribe(GET_ALL_DEVICE_STATUS_TOPIC, 1);
                client_.subscribe(OPERATE_PLC_WITH_VERIFY_TOPIC, 1);
                client_.subscribe(UPDATE_CONFIG, 1);

                reconnecting_ = false;
                return;
            }
            catch (const mqtt::exception& e) {
                std::cerr << "[MQTT] Reconnect failed: " << e.what() << std::endl;
                LOG_ERROR(("MQTT reconnect failed: " + std::string(e.what())).c_str());
                std::this_thread::sleep_for(std::chrono::seconds(sleepSec));
                sleepSec = std::min(60, sleepSec * 2);
            }
        }
    }).detach();
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
    if (!ensureConnected()) {
        LOG_ERROR("MQTT publish aborted: not connected");
        return;
    }

    try {
        auto msg = mqtt::make_message(topic, payload, qos, retained);
        client_.publish(msg);
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[MQTT] Publish failed: " << e.what() << std::endl;
        LOG_ERROR(("MQTT publish failed: " + std::string(e.what())).c_str());
    }
}


bool MqttService::ensureConnected()
{
    if (client_.is_connected())
        return true;

    // 如果后台重连正在进行，不再在这里阻塞尝试重连
    if (reconnecting_) {
        LOG_INFO("MQTT is reconnecting in background.");
        return false;
    }

    try {
        LOG_INFO("MQTT not connected, trying to reconnect...");
        client_.reconnect()->wait();

        // 重新订阅
        client_.subscribe(GET_REAL_IMAGE_TOPIC, 1);
        client_.subscribe(OPERATE_PLC_TOPIC, 1);
        client_.subscribe(UPDATE_CONFIG_TOPIC, 1);
        client_.subscribe(GET_SENSOR_DATA_TOPIC, 1);
        client_.subscribe(OPERATE_CAR_TOPIC, 1);
        client_.subscribe(GET_ALL_DEVICE_STATUS_TOPIC, 1);
        client_.subscribe(OPERATE_PLC_WITH_VERIFY_TOPIC, 1);
        client_.subscribe(UPDATE_CONFIG, 1);

        LOG_INFO("MQTT reconnect success.");
        return true;
    }
    catch (const mqtt::exception& e) {
        LOG_ERROR(("MQTT ensureConnected failed: " + std::string(e.what())).c_str());
        return false;
    }
}