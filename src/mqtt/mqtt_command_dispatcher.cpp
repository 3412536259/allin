#include "mqtt_command_dispatcher.h"
#include "Command.h"
#include "CommandTask.h"
#include "ConfigUtil.h"

MqttCommandDispatcher::MqttCommandDispatcher(JobScheduler& scheduler)
    :scheduler_(scheduler)
{
    ConfigUtil::loadBoxId(ConfigUtil::getConfigPath(), boxId_);
}

void MqttCommandDispatcher::onMessage(const std::string& topic, const std::string& payload)
{
    nlohmann::json j;
    try{
        j = nlohmann::json::parse(payload);
    }catch(...){
        std::cerr << "Invalid JSON: " << payload << std::endl;
        return;
    }
    std::string err;
    Command cmd;
    if (!Command::fromJson(j, boxId_, cmd, err)) {
        std::string cameraTopic = "box" + boxId_ + "/device/camera";
        if (topic == cameraTopic) {
            handleGetRealImage(j);
            return;
        }
        return;
    }

    auto task = std::make_shared<CommandTask>(cmd);
    int id = scheduler_.submit(task);


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

