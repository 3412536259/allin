#include "plc_manager.h"
#include "mock_plc_device.h"
#include <iostream>

PLCManager::PLCManager(){}

PLCManager::~PLCManager(){
    for(auto* dev : devices_){
        delete dev;
    }
}

bool PLCManager::start(){
    if(!loadConfig()){
        std::cerr<<"[PLCManager] Failed to load configuration."<<std::endl;
        return false;
    }
    if(!registerDevices()){
        std::cerr<<"[PLCManager] Failed to register devices."<<std::endl;
        return false;
    }

    for(auto* dev : devices_){
        auto id = dev->getId();
        PLCRuntimeState st;
        st.state = dev->queryStatus();

        std::lock_guard<std::mutex> lock(plcMutex_);
        deviceStateTable_[id] = st;
    }

    std::cout<<"[PLCManager] Started successfully with "<<devices_.size()<<" devices."<<std::endl;
    return true;
}

PLCInfo PLCManager::getStatus(const std::string& deviceId){
    std::lock_guard<std::mutex> lock(plcMutex_);
    PLCInfo info;
    info.id = deviceId;

    if(deviceStateTable_.count(deviceId) == 0){
        info.state = PLCState::OFFLINE;
        return info;
    }

    auto& st = deviceStateTable_[deviceId];
    info.state = st.state;
    return info;
}

std::vector<PLCInfo> PLCManager::getAllStatus(){
    std::lock_guard<std::mutex> lock(plcMutex_);
    std::vector<PLCInfo> plcList;
    plcList.reserve(deviceStateTable_.size());

    for(auto& kv : deviceStateTable_){
        PLCInfo info;
        info.id = kv.first;
        info.state = kv.second.state;
        
        plcList.push_back(info);
    }
    return plcList;
}

bool PLCManager::operate(const std::string& deviceId, const std::string& cmd){
    PLCDevice* targetDevice = nullptr;

    for(auto* dev : devices_){
        if(dev->getId() == deviceId){
            targetDevice = dev;
            break;
        }
    }

    if(!targetDevice){
        std::cerr<<"[PLCManager] Device "<<deviceId<<" not found."<<std::endl;
        return false;
    }

    bool result = targetDevice->operate(cmd);

    {
        std::lock_guard<std::mutex> lock(plcMutex_);
        auto& st = deviceStateTable_[deviceId];
        st.state = targetDevice->queryStatus();
    }

    std::cout << "[PLCManager] operate(" << deviceId
              << ", cmd=" << cmd << ") result=" << (result ? "SUCCESS" : "FAIL")
              << std::endl;

    return result;
}

bool PLCManager::loadConfig(){
    std::cout<<"[PLCManager] Loading configuration..."<<std::endl;
    return true;
}

bool PLCManager::registerDevices(){
    std::cout<<"[PLCManager] Registering devices..."<<std::endl;
    devices_.push_back(new MockPLCDevice("Valve1"));
    devices_.push_back(new MockPLCDevice("PumpA"));
    return true;
}