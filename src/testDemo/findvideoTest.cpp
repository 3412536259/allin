#include <iostream>
#include "find_video_url.h"
int main() {
    std::string url = findVideoUrl(
        "/home/ztl/workspace/allin/allin/video",
        "http://127.0.0.1:8080/videos",
        "10",
        "20251213",
        "11:15"
    );

    if (url.empty()) {
        std::cout << "no video found\n";
    } else {
        std::cout << url << std::endl;
    }
    return 0;
}
