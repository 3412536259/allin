#pragma once
#include "plc_manager.h"

class MockPLCDevice : public PLCDevice{
public:
    MockPLCDevice(const std::string& id) : id_(id){}

    PLCState queryStatus() override{
        return PLCState::ONLINE;
    }

    std::string getId() const override{
        return id_;
    }

private:
    std::string id_;
};