#pragma once
#include "plc_manager.h"
#include <iostream>

class MockPLCDevice : public PLCDevice{
public:
    MockPLCDevice(const std::string& id) : id_(id){}

    PLCState queryStatus() override{
        return state_;
    }

    std::string getId() const override{
        return id_;
    }

    bool operate(const std::string& cmd) override{
        std::cout<<"[MockPLCDevice] Operating command: "<<cmd<<" on device: "<<id_<<std::endl;

        if(cmd == "open"){
            state_ = PLCState::ONLINE;
            return true;
        } else if(cmd == "close"){
            state_ = PLCState::OFFLINE;
            return true;
        }
        return true;
    }

private:
    std::string id_;
    PLCState state_ = PLCState::ONLINE;
};