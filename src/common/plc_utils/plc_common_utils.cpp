#include "plc_common_utils.h"
#include <iostream>

/**
 * @brief 计算 Modbus RTU 规范的 CRC-16/MODBUS 校验码。
 * @param data 待校验的字节数组 (不含CRC)
 * @return CRC-16 校验码 (低位在前，高位在后)
 */
unsigned short calculate_crc16(const std::vector<char>& data) {
    unsigned short crc = 0xFFFF;
    for (char byte : data) {
        crc ^= (unsigned char)byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001; // 0x8005 反转
            } else {
                crc >>= 1;
            }
        }
    }
    // 返回未交换的 CRC 值，由调用者决定如何附加 (通常是低位在前)
    return crc;
}

/**
 * @brief 将 Hex 字符串转换为字节向量 (真实 Modbus RTU 必需)
 */
std::vector<char> HexStringToBytes(const std::string& hexFrame) {
    std::vector<char> bytes;
    std::stringstream ss(hexFrame);
    std::string byteString;
    while (ss >> byteString) {
        if (byteString.length() == 2) {
            try {
                bytes.push_back(static_cast<char>(std::stoul(byteString, nullptr, 16)));
            } catch (const std::exception& e) {
                std::cerr << "Hex conversion error for byte: " << byteString << std::endl;
            }
        }
    }
    return bytes;
}

