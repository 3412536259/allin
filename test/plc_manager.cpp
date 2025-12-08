#include "plc_manager.h"
#include "config_parser.h"
#include <iostream>
#include <algorithm>

PLCManager::PLCManager(){
    // 1.从 Config 中获取总配置数据
    DeviceConfigRoot rootConfig = ConfigParser::getInstance().getConfig();

    // 2.初始化配置映射和连接器
    for(const auto& plcConfig : rootConfig.plcs){
        plcConfigs_[plcConfig.plcId] = plcConfig;

        std::unique_ptr<IPLCConnector> connector;

        if(plcConfig.connectionType == "direct"){
            // connector = std::make_unique<SerialPLCConnector>(plcConfig);
            std::cout<<"[PLCManager] create serial plc connector\n";
        }
        else if(plcConfig.connectionType == "gateway"){
            // connector = std::make_unique<GatewayTCPConnector>(plcConfig);
            std::cout<<"[PLCManager] create gateway plc connector\n";
        }
        else{
            std::cerr << "[PLCManager] Unknown connection type: '" << plcConfig.connectionType << "' for PLC ID: " << plcConfig.plcId <<"\n";
            continue;
        }
        plcConnectors_[plcConfig.plcId] = std::move(connector);
    }
}