#include "idevice_manager.h"
#include "device_manager.h"
#include "config_parser.h"
#include "task.h"
#include "JobScheduler.h"
#include "my_mqtt_callback.h"
#include "task_result_publisher.h"
#include "mqtt_service.h"
#include <memory>
#include <thread>
int main()
{
    ConfigParser::getInstance().loadFromFile("/home/ztl/workspace/allin/allin/include/common/config/config.json");
    std::shared_ptr<IDeviceManager> ideviceManager = std::make_shared<DeviceManager>();
    // std::this_thread::sleep_for(std::chrono::seconds(5)); //等待设备注册初始化完成
    // ideviceManager->getStatus();
    // std::this_thread::sleep_for(std::chrono::seconds(5)); 
    JobScheduler jobscheduler(8,ideviceManager.get(),nullptr);
    MqttCommandDispatcher cmdDispatcher(jobscheduler);  //根据接收的主题来选择调用的处理任务，需要依赖jobscheduler的接口提交任务
    MqttService mqtt("mqtt://broker.emqx.io:1883", "edge-box", &cmdDispatcher); //需要依赖cmdDispatcher分发相应任务
    MqttPublisher publisher(&mqtt);
    jobscheduler.setPublisher(&publisher); //依赖publisher的唯一原因是需要将publisher传入Taskcontext供具体task调用
    std::cout << "System running..." << std::endl;
    while (true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }

}