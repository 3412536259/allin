#include "mqtt_command_dispatcher.h"


MqttCommandDispatcher::MqttCommandDispatcher(JobScheduler& scheduler)
    :scheduler_(scheduler){}
void MqttCommandDispatcher::onMqttMessage(const std::string& topic, const std::string& payload)
{
    nlohmann::json j;
    try{
        j = nlohmann::json::parse(payload);
    }catch(...){
        std::cerr << "Invalid JSON: " << payload << std::endl;
        return;
    }
    // 用 MQTT topic 决定任务类型
    if (topic == "device/camera/getRealImage") {
        handleGetRealImage(j);
    }
    else if (topic == "device/plc/operate") {
        handleOperatePlc(j);
    }
    else if (topic == "device/config/update") {
        handleUpdateConfig(j);
    }
    else {
        std::cout << "Unknown topic: " << topic << std::endl;
    }


}

void MqttCommandDispatcher::handleGetRealImage(const nlohmann::json& j)
{
    if (!j.contains("deviceId")) return;

    std::string camId = j["deviceId"];

    auto task = std::make_shared<GetCameraRealImageTask>(camId);
    int id = scheduler_.submit(task);

    std::cout << "Submitted GetRealImageTask id=" << id 
              << " for cam=" << camId << std::endl;
}
void MqttCommandDispatcher::handleOperatePlc(const nlohmann::json& j)
{

}
void MqttCommandDispatcher::handleUpdateConfig(const nlohmann::json& j)
{

}
