#include "WebController.h"
#include <iostream>
#include "image_processor.h"
#include "task.h"
#include "task_result_publisher.h"
#include <sys/stat.h>

const std::string STATIC_FILE_ROOT = "/home/ztl/workspace/allin/videos";
const std::string DOWNLOAD_URL_ROOT_PATH = "/videos/";

WebController::WebController(const std::string& /*configPath*/, IDeviceManager* devMgr, JobScheduler* scheduler)
    : devMgr_(devMgr), scheduler_(scheduler)
{
}

json WebController::handleJson(const json& payload)
{
    json resp;
    resp["success"] = false;
    resp["error"] = "use handleHttp for routing";
    return resp;
}

json WebController::handleHttp(const std::string& path, const json& payload)
{
    json resp;

    if (!scheduler_) {
        resp["success"] = false;
        resp["error"] = "scheduler not available";
        return resp;
    }

    // -------------------------------
    // 1) Camera getRealImage  → 提交任务
    // -------------------------------
    if (path.find("/device/camera/getRealImage") != std::string::npos) {
        std::string camId = payload.value("cameraId", payload.value("cameraId", ""));
        if (camId.empty()) {
            resp["success"] = false;
            resp["error"] = "no camera id";
            return resp;
        }

        auto task = std::make_shared<GetCameraRealImageTask>(camId);
        int id = scheduler_->submit(task, "http");

        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }

    // -------------------------------
    // 2) PLC operate → 提交任务
    // -------------------------------
    if (path.find("/device/plc/operate") != std::string::npos ||
        path.find("/device/control/solenoid") != std::string::npos) {

        std::string deviceId = payload.value("deviceId", payload.value("plc_device_id", ""));
        std::string action = payload.value("action", "");

        if (deviceId.empty()) {
            resp["success"] = false;
            resp["error"] = "no device id";
            return resp;
        }

        auto task = std::make_shared<OperateValveTask>(deviceId, action);
        int id = scheduler_->submit(task, "http");

        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }

    // -------------------------------
    // 3) 车控制 → 提交任务
    // -------------------------------
    if (path.find("/device/carControl") != std::string::npos) {

        auto task = std::make_shared<CarControlTask>(payload);
        int id = scheduler_->submit(task, "http");

        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }
    /*
    // -------------------------------
    // 4) get sensor → 提交任务
    // -------------------------------
    // if (path.find("/device/sensor/get") != std::string::npos) {

    //     if (!payload.contains("sensorId")) {
    //         resp["success"] = false;
    //         resp["error"] = "missing sensorId";
    //         return resp;
    //     }

    //     auto task = std::make_shared<GetSensorDataTask>(payload["sensorId"].get<std::string>());
    //     int id = scheduler_->submit(task, "http");

    */
    //     resp["success"] = true;
    //     resp["task_id"] = id;
    //     return resp;
    // }

    // 获取全部状态
    if(path.find("/device/getAll") != std::string::npos) {
        auto task = std::make_shared<GetDeviceStatusTask>();
        int id = scheduler_->submit(task, "http");
        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }

    // 视频下载路由
    if(path.find("device/video/download") != std::string::npos){
        std::string channel = payload.value("channel", "");
        std::string date = payload.value("date", "");
        std::string timeStr = payload.value("time", "");

        if(channel.empty() || date.empty() || timeStr.empty()){
            resp["success"] = false;
            resp["error"] = "missing params: channel/date/time";
            return resp;
        }

        auto task = std::make_shared<DownloadVideoTask>(channel, date, timeStr);
        int id = scheduler_->submit(task, "http");
        resp["success"] = true;
        resp["task_id"] = id;
        return resp;
    }

    // -------------------------------
    // 5) 其它未知路径，统一接受但不执行任务
    // -------------------------------
    resp["success"] = true;
    resp["note"] = "accepted";
    return resp;
}

// 辅助函数：根据文件扩展名获取 MIME 类型
std::string getMimeType(const std::string& path) {
    static const std::map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".mp4", "video/mp4"}, // 视频文件类型
        {".txt", "text/plain"}
        // ... 添加更多需要的类型 ...
    };
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        std::string ext = path.substr(dot_pos);
        auto it = mime_types.find(ext);
        if (it != mime_types.end()) {
            return it->second;
        }
    }
    return "application/octet-stream"; // 默认类型
}

bool WebController::handleStaticFile(int client_fd, const std::string& relativePath) {
    
    // 1. 构造本地文件系统路径
    
    // 移除 URL 中的 /videos/ 部分 (例如：/videos/10/20251213/file.mp4 -> 10/20251213/file.mp4)
    std::string path_after_root = relativePath.substr(DOWNLOAD_URL_ROOT_PATH.length());
    
    // **安全检查**
    if (path_after_root.empty() || path_after_root.find("..") != std::string::npos) {
        std::cerr << "Security warning: Invalid path or traversal attempt: " << relativePath << std::endl;
        return false; 
    }

    // 构造最终的绝对路径: /data/ + 10/20251213/file.mp4
    std::string fullPath = STATIC_FILE_ROOT + "/" + path_after_root;
    
    // 2. 检查文件是否存在且可读
    struct stat file_info;
    if (stat(fullPath.c_str(), &file_info) != 0 || !S_ISREG(file_info.st_mode)) {
        std::cerr << "File not found or is directory: " << fullPath << std::endl;
        // 简单发送 404 响应 (在 WebService::handleClient 中发送)
        return false;
    }

    // 3. 打开文件
    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << fullPath << std::endl;
        return false;
    }

    // 4. 构建 HTTP 响应头
    std::string mimeType = getMimeType(fullPath);
    size_t fileSize = static_cast<size_t>(file_info.st_size);

    std::ostringstream header;
    header << "HTTP/1.1 200 OK\r\n";
    header << "Content-Type: " << mimeType << "\r\n";
    header << "Content-Length: " << fileSize << "\r\n";
    header << "Connection: close\r\n\r\n";
    
    std::string headerStr = header.str();
    
    // 5. 发送头部
    if (send(client_fd, headerStr.c_str(), headerStr.size(), 0) < 0) {
        std::cerr << "Failed to send header for file: " << fullPath << std::endl;
        return false;
    }
    
    // 6. 发送文件内容
    // 注意：这里的实现是简单的逐块读取和发送，适用于小文件。
    // 对于大文件，推荐使用 sendfile() 系统调用，但这需要修改 WebService::handleClient 的签名。
    char buffer[4096];
    bool success = true;
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        ssize_t bytes_to_send = file.gcount();
        if (send(client_fd, buffer, bytes_to_send, 0) < 0) {
            std::cerr << "Failed to send file content: " << fullPath << std::endl;
            success = false;
            break;
        }
    }
    
    std::cout << "Successfully served static file: " << fullPath << std::endl;
    return success;
}