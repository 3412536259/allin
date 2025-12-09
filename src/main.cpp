#include "idevice_manager.h"
#include "device_manager.h"
#include "config_parser.h"
#include "task.h"
#include "JobScheduler.h"
#include "my_mqtt_callback.h"
#include "mqtt_service.h"
#include "mqtt_command_dispatcher.h" 
#include "task_result_publisher.h"
#include <memory>
#include <thread>
#include "WebService.h"
#include "ConfigUtil.h"

int main()
{
    av_log_set_level(AV_LOG_QUIET);
    ConfigParser::getInstance().loadFromFile("/home/ztl/workspace/allin-develop/include/common/config/config.json");
    std::shared_ptr<IDeviceManager> ideviceManager = std::make_shared<DeviceManager>();
 
    JobScheduler jobscheduler(8,ideviceManager.get(),nullptr);
    std::string boxId;
    ConfigUtil::loadBoxId(ConfigUtil::getConfigPath(), boxId);
    
    std::string serverURI = "tcp://broker.emqx.io:1883";  
    std::string clientId = "allin_client";           
    MqttCommandDispatcher commandDispatcher(jobscheduler);
    MqttService mqttService(serverURI, clientId, jobscheduler, boxId, &commandDispatcher);
    mqttService.start();
    
    WebService ws("include/common/config/config.json", 8080, ideviceManager.get(), &jobscheduler);
    MqttPublisher publisher(&mqttService);  
    jobscheduler.setPublisher(&publisher); 
    
    ws.start();
    std::cout << "System running..." << std::endl;
    while (true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }

    return 0;
}