#pragma once

#include "Command.h"
#include "itask.h"



class CommandTask : public ITask {
public:
    CommandTask(const Command& cmd) : cmd_(cmd) {}
    std::string name() const override { return "CommandTask"; }
    void run(TaskContext& ctx) override;

private:
    Command cmd_;
};

