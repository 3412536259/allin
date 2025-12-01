#include "camera_manager.h"
#include <iostream>
int main(){
    std::shared_ptr<ICameraManager> cameras = std::make_shared<CameraManager>();
    cameras->start();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    auto cameraStatuses = cameras->getAllStatus();
    
    for(auto& s: cameraStatuses){
        std::cout << s.first << s.second.online_status << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(20));
}
