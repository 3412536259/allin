#include "idevice_manager.h"
#include "device_manager.h"
#include "config_parser.h"
#include <memory>
#include <thread>
int main()
{
    ConfigParser::getInstance().loadFromFile("/home/ztl/workspace/allin/allin/include/common/config/config.json");
    std::shared_ptr<IDeviceManager> ideviceManager = std::make_shared<DeviceManager>();
    std::this_thread::sleep_for(std::chrono::seconds(5)); //等待设备注册初始化完成
    ideviceManager->getStatus();
    std::this_thread::sleep_for(std::chrono::seconds(5)); 
}