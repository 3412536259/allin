#pragma once

#include "plc_info.h"
#include <string>


class PLCDevice{
public:
    virtual ~PLCDevice() = default;
    virtual PLCState queryStatus() = 0;
    virtual std::string getId() const = 0;
    virtual std::string getType() const = 0;
    virtual OperateResult operate(const std::string& cmd) = 0;
};