#pragma once

#include <string>
#include "config_info.h"

class IPLCConnector{
public:
    virtual ~IPLCConnector() = default;
    
    virtual bool link() = 0;
    virtual void disconnect() = 0;
    virtual std::string getConnectionStatus() const = 0;
    virtual std::string readRegister(const std::string& registerAddress) = 0;
    virtual std::string writeRegister(const std::string& registerAddress, const std::string& value) = 0;

protected:
    PLCConfig config_;
}