#include "idevice_manager.h"
#include "device_manager.h"
#include <memory>
#include <thread>
int main()
{
    std::shared_ptr<IDeviceManager> ideviceManager = std::make_shared<DeviceManager>();
    std::this_thread::sleep_for(std::chrono::seconds(5)); //等待设备注册初始化完成
    ideviceManager->getStatus();
    std::this_thread::sleep_for(std::chrono::seconds(5)); 
}