#include "idevice_manager.h"
#include "device_manager.h"
#include "config_parser.h"
#include "task.h"
#include "JobScheduler.h"
#include "my_mqtt_callback.h"
#include "mqtt_service.h"
#include <memory>
#include <thread>
int main()
{
    av_log_set_level(AV_LOG_QUIET);
    ConfigParser::getInstance().loadFromFile("/home/ztl/workspace/allin-develop/include/common/config/config.json");
    std::shared_ptr<IDeviceManager> ideviceManager = std::make_shared<DeviceManager>();
    // std::this_thread::sleep_for(std::chrono::seconds(5)); //等待设备注册初始化完成
    // ideviceManager->getStatus();
    // std::this_thread::sleep_for(std::chrono::seconds(5)); 
    JobScheduler jobscheduler(8,ideviceManager.get(),nullptr);
    MqttService mqtt("tcp://broker.emqx.io:1883", "edge-box", jobscheduler);
    jobscheduler.setMqtt(&mqtt);
    mqtt.start();   // 连接 + 订阅 + 进入稳定状态

    std::cout << "System running..." << std::endl;
    while (true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }

}