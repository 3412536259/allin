#include "FileUtils.h"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace fs = std::filesystem;

std::string FileUtils::generateFileName(
    const std::string& cameraDir,
    const std::string& ext)
{
    // 2. 获取当前系统时间（本地时间）
    std::time_t now = std::time(nullptr);
    std::tm* tmNow = std::localtime(&now);
    if (!tmNow) {
        throw std::runtime_error("Failed to get local time");
    }

    // 3. 拼接完整目录路径：cameraDir + / + 日期目录（如 /data/videos/摄像头01/20251116）
    std::stringstream dirSs;
    dirSs << cameraDir << "/"
          << std::put_time(tmNow, "%Y%m%d");  // 日期目录：YYYYMMDD（20251116）

    std::string fullDir = dirSs.str();
    // 4. 确保多级目录存在（cameraDir 若不存在也会自动创建，兜底）
    if (!ensureDirExists(fullDir)) {
        throw std::runtime_error("Failed to create directory: " + fullDir);
    }

    // 5. 拼接最终文件路径：完整目录/具体时间.mp4（如 143025.mp4）
    std::stringstream fileSs;
    fileSs << fullDir << "/"
           << std::put_time(tmNow, "%H_%M")  // 文件名仅保留具体时间（HHMMSS）
           << "." << ext;

    return fileSs.str();
}


bool FileUtils::ensureDirExists(const std::string& path)
{
    try {
        if (!fs::exists(path)) {
            fs::create_directories(path);
        }
        return true;
    } catch (...) {
        return false;
    }
}