#include "camera_manager.h"
#include "config_parser.h"
#include <iostream>
int main(){
    ConfigParser::getInstance().loadFromFile("/home/ztl/workspace/allin/allin/include/common/config/config.json");

    auto& config = ConfigParser::getInstance().getConfig();
    for(auto& kv : config.cameras)
    {
        std::cout << kv.id << kv.name << kv.url << std::endl;
    }

    // std::shared_ptr<ICameraManager> cameras = std::make_shared<CameraManager>();
    // cameras->start();
    // std::this_thread::sleep_for(std::chrono::seconds(5));
    // auto cameraStatuses = cameras->getAllStatus();

    // std::this_thread::sleep_for(std::chrono::seconds(20));
}
