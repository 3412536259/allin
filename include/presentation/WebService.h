#pragma once

#include <string>
#include <thread>
#include <memory>
#include "WebController.h"
#include "idevice_manager.h"
#include "JobScheduler.h"



class WebService {
private:
    WebController m_controller;
    int m_port;
    std::thread m_thread;
    int m_server_fd = -1;
    bool m_running = false;

public:
    WebService(const std::string& configPath, int port, IDeviceManager* devMgr, JobScheduler* scheduler);
    ~WebService();

    bool start();
    void stop();

private:
    void run();
    void handleClient(int client_fd);
};


