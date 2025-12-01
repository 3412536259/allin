#pragma once
#include <string>
#include "plc_info.h"
#include "iplc_device.h"
#include "plc_manager.h"
#include <iostream>

class MockPLCDevice : public PLCDevice{
public:
    MockPLCDevice(const std::string& id) : id_(id){}

    PLCState queryStatus() override{
        // 给设备发具体报文查询状态
        return state_;
    }

    std::string getId() const override{
        return id_;
    }

    std::string getType() const override{
        return "Mock";
    }

    OperateResult operate(const std::string& cmd) override{
        std::cout<<"[MockPLCDevice] Operating command: "<<cmd<<" on device: "<<id_<<std::endl;

        if(cmd == "open"){
            state_ = PLCState::ONLINE;
            return OperateResult::SUCCESS;
        } else if(cmd == "close"){
            state_ = PLCState::OFFLINE;
            return OperateResult::SUCCESS;
        }
        return OperateResult::SUCCESS;
    }

private:
    std::string id_;
    PLCState state_ = PLCState::ONLINE;
};