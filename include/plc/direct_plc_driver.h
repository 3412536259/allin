#pragma once
#include "plc_driver.h"
#include <string>

class DirectPLCDriver : public PLCDriver{
public:
    DirectPLCDriver(const PLCConfig& cfg);
    ~DirectPLCDriver() override;

    bool start() override;
    void stop() override;

    bool readRegister(const std::string& addr, int& outValue) override;
    bool writeRegister(const std::string& addr, int value) override;

    PLCState getState() override { return state_; }
private:
    PLCConfig config_;
    int serialFd_ = -1;
    PLCState state_ = PLCState::OFFLINE;
};