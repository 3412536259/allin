#include "WebService.h"
#include "json.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include "logger.h"


using nlohmann::json;
const std::string DOWNLOAD_URL_ROOT_PATH = "/videos/";

WebService::WebService(const std::string& configPath, int port, IDeviceManager* devMgr, JobScheduler* scheduler)
    : m_controller(configPath, devMgr, scheduler), m_port(port) {}

WebService::~WebService() {
    stop();
}

bool WebService::start() {
    if (m_running) return true;

    m_server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd < 0) {
        std::cerr << "WebService: Failed to create server socket" << std::endl;
        LOG_ERROR("WebService: Failed to create server socket");
        return false;
    }

    int opt = 1;
    setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(m_port));

    if (bind(m_server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "WebService: Failed to bind server socket" << std::endl;
        LOG_ERROR("WebService: Failed to bind server socket");
        ::close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    if (listen(m_server_fd, 4) < 0) {
        std::cerr << "WebService: Failed to listen on server socket" << std::endl;
        LOG_ERROR("WebService: Failed to listen on server socket");
        ::close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    m_running = true;
    m_thread = std::thread(&WebService::run, this);
    std::cout << "WebService listening on port " << m_port << std::endl;
    LOG_INFO("WebService listening on port " + std::to_string(m_port));
    return true;
}

void WebService::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_server_fd >= 0) {
        ::shutdown(m_server_fd, SHUT_RDWR);
        ::close(m_server_fd);
        m_server_fd = -1;
    }
    if (m_thread.joinable()) m_thread.join();
}

void WebService::run() {
    while (m_running) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(m_server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (m_running) {
                std::cerr << "WebService: Accept failed" << std::endl;
                LOG_ERROR("WebService: Accept failed");
            }
            break;
        }
        std::thread(&WebService::handleClient, this, client_fd).detach();
    }
}

void WebService::handleClient(int client_fd) {
    constexpr size_t bufsize = 8192;
    std::string request;
    request.reserve(bufsize);
    char buffer[bufsize];
    // read initial data (headers + maybe body)
    ssize_t n = recv(client_fd, buffer, bufsize - 1, 0);
    if (n <= 0) {
        ::close(client_fd);
        return;
    }
    buffer[n] = '\0';
    request.append(buffer, static_cast<size_t>(n));

    // parse request line to get method and path
    std::istringstream reqstream(request);
    std::string requestLine;
    std::getline(reqstream, requestLine);
    std::string method, path, httpver;
    {
        std::istringstream rl(requestLine);
        rl >> method >> path >> httpver;
    }

    if (method == "GET" && path.rfind(DOWNLOAD_URL_ROOT_PATH, 0) == 0) { 
        std::cout << "Handling static file request: " << path << std::endl;

        // 调用 WebController 中的静态文件处理逻辑
        if (m_controller.handleStaticFile(client_fd, path)) {
            // 文件流已发送，成功！关闭连接并退出。
            ::shutdown(client_fd, SHUT_RDWR);
            ::close(client_fd);
            return; 
        } else {
            // 文件不存在或处理失败，发送 404 响应
            std::string not_found_resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            send(client_fd, not_found_resp.c_str(), not_found_resp.size(), 0);
            ::shutdown(client_fd, SHUT_RDWR);
            ::close(client_fd);
            return;
        }
    }

    std::string::size_type pos = request.find("Content-Length:");
    size_t content_length = 0;
    if (pos != std::string::npos) {
        pos += strlen("Content-Length:");
        auto end = request.find('\r', pos);
        if (end != std::string::npos) {
            std::string lenstr = request.substr(pos, end - pos);
            try { content_length = std::stoul(lenstr); } catch (...) { content_length = 0; }
        }
    }

    auto body_pos = request.find("\r\n\r\n");
    std::string body;
    if (body_pos != std::string::npos) {
        body = request.substr(body_pos + 4);
        while (body.size() < content_length) {
            ssize_t rn = recv(client_fd, buffer, bufsize - 1, 0);
            if (rn <= 0) break;
            body.append(buffer, static_cast<size_t>(rn));
        }
    }

    json respJson;
    try {
        json root = json::parse(body.empty() ? "{}" : body);

        // Route HTTP path to controller handler (controller will map to same tasks as MQTT)
        respJson = m_controller.handleHttp(path, root);
    } catch (const std::exception& e) {
        respJson["success"] = false;
        respJson["error"] = std::string("invalid_json: ") + e.what();
    }

    std::string out = respJson.dump();

    std::ostringstream reply;
    reply << "HTTP/1.1 200 OK\r\n";
    reply << "Content-Type: application/json\r\n";
    reply << "Content-Length: " << out.size() << "\r\n";
    reply << "Connection: close\r\n\r\n";
    reply << out;

    std::string repstr = reply.str();
    send(client_fd, repstr.c_str(), repstr.size(), 0);
    ::shutdown(client_fd, SHUT_RDWR);
    ::close(client_fd);
}
