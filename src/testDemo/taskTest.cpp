#include "idevice_manager.h"
#include "device_manager.h"
#include "config_parser.h"
#include "task.h"
#include "JobScheduler.h"
#include "my_mqtt_callback.h"
#include <memory>
#include <thread>
int main()
{
    ConfigParser::getInstance().loadFromFile("/home/ztl/workspace/allin/allin/include/common/config/config.json");
    // std::shared_ptr<IDeviceManager> ideviceManager = std::make_shared<DeviceManager>();
    // std::this_thread::sleep_for(std::chrono::seconds(5)); //等待设备注册初始化完成
    // ideviceManager->getStatus();
    // std::this_thread::sleep_for(std::chrono::seconds(5)); 
    JobScheduler jobscheduler(8);
     MqttCommandDispatcher dispatcher(jobscheduler);

    mqtt::async_client client("mqtt://broker.emqx.io:1883", "edge-box");
    MyMqttCallback cb(dispatcher);
    client.set_callback(cb);

    client.connect()->wait();

    client.subscribe("device/camera/getRealImage", 1);
    client.subscribe("device/plc/operate", 1);
    client.subscribe("device/config/update", 1);

    std::cout << "MQTT ready. Waiting for commands..." << std::endl;

    while (true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }

}