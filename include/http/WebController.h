#pragma once

#include <string>
#include "json.hpp"
#include "task.h"
#include "idevice_manager.h"
#include "JobScheduler.h"

using nlohmann::json;


class WebController {
public:
    WebController(const std::string& configPath, IDeviceManager* devMgr, JobScheduler* scheduler);
    json handleJson(const json& payload);

    json handleHttp(const std::string& path, const json& payload);

private:
    std::string boxId_;
    IDeviceManager* devMgr_ = nullptr;
    JobScheduler* scheduler_ = nullptr;
};

