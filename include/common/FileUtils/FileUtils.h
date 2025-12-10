#ifndef FILE_UTILS_H
#define FILE_UTILS_H
#include <string>

class FileUtils {
public:
    /**
     * 生成格式：cameraDir（绝对路径）/日期目录/具体时间.mp4（例：/data/videos/摄像头01/20251116/143025.mp4）
     * @param cameraDir 摄像头绝对路径（已包含摄像头名，如 "/data/videos/摄像头01"）
     * @param ext 文件后缀（默认 mp4）
     * @return 完整的文件路径
     * @throw std::runtime_error 时间获取失败或目录创建失败时抛出
     */
    static std::string generateFileName(
        const std::string& cameraDir,
        const std::string& ext = "mp4");
    // 检查并创建目录
    static bool ensureDirExists(const std::string& path);
};

#endif