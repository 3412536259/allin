#include "plc_manager.h"
#include "mock_plc_device.h"
#include <iostream>

PLCManager::PLCManager(){}

PLCManager::~PLCManager(){
    for(auto* dev : devices_){
        delete dev;
    }
}

void PLCManager::setConfigs(const std::vector<PLCConfig>& cfgs){
    deviceConfigs_ = cfgs;
}

bool PLCManager::start(){
    if(deviceConfigs_.empty()){
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
        st.type = dev->getType();

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
        info.type = "Unknown";
        return info;
    }

    auto& st = deviceStateTable_[deviceId];
    info.state = st.state;
    info.type = st.type;
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
        info.type = kv.second.type;
        
        plcList.push_back(info);
    }
    return plcList;
}

OperateResult PLCManager::operate(const std::string& deviceId, const std::string& cmd){
    PLCDevice* targetDevice = nullptr;

    for(auto* dev : devices_){
        if(dev->getId() == deviceId){
            targetDevice = dev;
            break;
        }
    }

    if(!targetDevice){
        std::cerr<<"[PLCManager] Device "<<deviceId<<" not found."<<std::endl;
        return OperateResult::FAILED;
    }

    OperateResult result = targetDevice->operate(cmd);

    {
        std::lock_guard<std::mutex> lock(plcMutex_);
        auto& st = deviceStateTable_[deviceId];
        st.state = targetDevice->queryStatus();
    }

    const char* rstr =
        (result == OperateResult::SUCCESS) ? "SUCCESS" :
        (result == OperateResult::TIMEOUT) ? "TIMEOUT" : "FAILED";

    std::cout << "[PLCManager] operate(" << deviceId
            << ", cmd=" << cmd << ") result=" << rstr << std::endl;


    return result;
}

bool PLCManager::loadConfig(){
    std::cout<<"[PLCManager] Loading configuration..."<<std::endl;
    // 模拟加载配置
    PLCConfig cfg1;
    cfg1.id = "Valve1";
    cfg1.type = "SolenoidValve";
    cfg1.serialPort = "/dev/ttyS4";
    cfg1.baudrate = 9600;
    cfg1.slaveId = 1;
    cfg1.regValve = 0x0504;
    deviceConfigs_.push_back(cfg1);
    PLCConfig cfg2;
    cfg2.id = "PumpA";
    cfg2.type = "Mock";
    cfg2.serialPort = "COM2";
    cfg2.baudrate = 115200;
    cfg2.slaveId = 2;
    cfg2.regValve = 0x0600;
    deviceConfigs_.push_back(cfg2);
    return true;
}

bool PLCManager::registerDevices(){
    std::cout<<"[PLCManager] Registering devices..."<<std::endl;
    for(const auto& cfg : deviceConfigs_){
        PLCDevice* dev = PLCDeviceFactory::createDevice(cfg);
        if(!dev){
            std::cerr<<"[PLCManager] Unknown device type: "<<cfg.type<<std::endl;
            return false;
        }
        devices_.push_back(dev);
        std::cout<<"[PLCManager] Registered device: "<<cfg.id<<" type="<<cfg.type<<std::endl;
    }
    return true;
}