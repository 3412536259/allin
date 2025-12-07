#pragma once

#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>

unsigned short calculate_crc16(const std::vector<char>& data);
std::vector<char> HexStringToBytes(const std::string& hexFrame);
/**
 * @brief 将字节数组转换为 Hex 字符串用于打印日志
 */
inline std::string BytesToHexString(const std::vector<char>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : data) {
        ss << std::setw(2) << (static_cast<int>(byte) & 0xFF) << " ";
    }
    std::string result = ss.str();
    if (!result.empty()) {
        result.pop_back(); // 移除末尾空格
    }
    // 确保所有字符为大写，便于日志阅读
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}