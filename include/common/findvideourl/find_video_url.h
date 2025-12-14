#ifndef FIND_VIDEO_URL_H
#define FIND_VIDEO_URL_H

#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

const std::string baseDir = "/home/ztl/workspace/allin/videos";
const std::string baseUrl = "http://127.0.0.1:8081/videos";

std::string findVideoUrl(
    const std::string& channel,   // 10
    const std::string& date,      // 20251213
    const std::string& timeStr    // 11:06
) {
    int reqHour = 0, reqMin = 0;
    if (sscanf(timeStr.c_str(), "%d:%d", &reqHour, &reqMin) != 2)
        return "";

    int targetMin = reqHour * 60 + reqMin;

    fs::path dirPath = fs::path(baseDir) / channel / date;
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
        return "";

    int bestMin = -1;
    std::string bestFile;

    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file())
            continue;

        std::string name = entry.path().filename().string();

        int h = 0, m = 0;
        if (sscanf(name.c_str(), "%d_%d.mp4", &h, &m) != 2)
            continue;

        int fileMin = h * 60 + m;
        if (fileMin <= targetMin && fileMin > bestMin) {
            bestMin = fileMin;
            bestFile = name;
        }
    }

    if (bestMin < 0)
        return "";

    // 拼接最终 URL
    return baseUrl + "/" + channel + "/" + date + "/" + bestFile;
}





#endif