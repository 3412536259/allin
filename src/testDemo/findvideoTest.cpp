#include <iostream>
#include "find_video_url.h"
int main() {
    std::string url = findVideoUrl(
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
