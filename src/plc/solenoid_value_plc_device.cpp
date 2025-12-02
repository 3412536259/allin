#include "solenoid_valve_plc_device.h"
#include <chrono>

SolenoidValvePLCDevice::SolenoidValvePLCDevice(const PLCConfig& cfg)
    : cfg_(cfg) {
    openSerial();
}

SolenoidValvePLCDevice::~SolenoidValvePLCDevice() {
    if (serialFd_ != -1) {
        close(serialFd_);
    }
}

PLCState SolenoidValvePLCDevice::queryStatus() {
    return state_;
}

std::string SolenoidValvePLCDevice::getId() const {
    return cfg_.id;
}

std::string SolenoidValvePLCDevice::getType() const {
    return cfg_.type;
}

OperateResult SolenoidValvePLCDevice::operate(const std::string& cmd) {
    if (cmd == "open") {
        return doWriteValue(true);
    } else if (cmd == "close") {
        return doWriteValue(false);
    } else {
        std::cerr << "[SolenoidValvePLCDevice] Unknown command: " << cmd << std::endl;
        return OperateResult::FAILED;
    }
}

OperateResult SolenoidValvePLCDevice::doWriteValue(bool open){
    auto req = buildWriteCoilRequest(open);
    if (!sendFrame(req)) {
        return OperateResult::FAILED;
    }

    std::vector<uint8_t> resp;
    auto ret = recvFrame(resp, 1000);
    if (ret != OperateResult::SUCCESS) {
        return ret;
    }

    if (!validateResponse(req, resp)) {
        std::cerr << "[SolenoidValvePLCDevice] Invalid response received." << std::endl;
        return OperateResult::FAILED;
    }

    state_ = open ? PLCState::ONLINE : PLCState::OFFLINE;
    return OperateResult::SUCCESS;
}

std::vector<uint8_t> SolenoidValvePLCDevice::buildWriteCoilRequest(bool value) {
    std::vector<uint8_t> frame(8);
    frame[0] = static_cast<uint8_t>(cfg_.slaveId);
    frame[1] = 0x05; // Write Single Coil
    frame[2] = (cfg_.regValve >> 8) & 0xFF;
    frame[3] = cfg_.regValve & 0xFF;
    frame[4] = value ? 0xFF : 0x00;
    frame[5] = 0x00;

    uint16_t crc = crc16(frame.data(), 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;

    return frame;
}

bool SolenoidValvePLCDevice::validateResponse(const std::vector<uint8_t>& req,
                                              const std::vector<uint8_t>& resp) 
{
    if (resp.size() != 8) return false;

    // CRC check
    uint16_t crc = crc16(resp.data(), 6);
    if (resp[6] != (crc & 0xFF) || resp[7] != (crc >> 8))
        return false;

    // 前 6 字节必须一致
    for (int i = 0; i < 6; i++)
        if (req[i] != resp[i])
            return false;

    return true;
}

bool SolenoidValvePLCDevice::sendFrame(const std::vector<uint8_t>& frame) {
    int written = write(serialFd_, frame.data(), frame.size());
    if (written != (int)frame.size()) {
        std::cerr << "[Valve] write error\n";
        return false;
    }

    std::cout << "[Valve] Send: ";
    printHex(frame.data(), frame.size());
    return true;
}

OperateResult SolenoidValvePLCDevice::recvFrame(std::vector<uint8_t>& out, int timeoutMs) {
    const int expectLen = 8;
    out.clear();
    out.reserve(expectLen);

    uint8_t buf[expectLen];

    using clock = std::chrono::steady_clock;
    auto startTime = clock::now();

    int totalRead = 0;

    while (totalRead < expectLen) {
        // 计算剩余时间
        auto now = clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        int remain = timeoutMs - elapsed;

        if (remain <= 0)
            return OperateResult::TIMEOUT;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(serialFd_, &rfds);

        struct timeval tv;
        tv.tv_sec = remain / 1000;
        tv.tv_usec = (remain % 1000) * 1000;

        int ret = select(serialFd_ + 1, &rfds, NULL, NULL, &tv);
        if (ret == 0)
            return OperateResult::TIMEOUT;
        if (ret < 0)
            return OperateResult::FAILED;

        int n = read(serialFd_, buf + totalRead, expectLen - totalRead);
        if (n <= 0)
            return OperateResult::FAILED;

        totalRead += n;
    }

    out.assign(buf, buf + expectLen);

    std::cout << "[Valve] Recv: ";
    printHex(buf, expectLen);

    return OperateResult::SUCCESS;
}

void SolenoidValvePLCDevice::openSerial() {
    serialFd_ = open(cfg_.direct.serial.port.c_str(), O_RDWR | O_NOCTTY);

    if (serialFd_ < 0) {
        std::cerr << "[Valve] Failed to open " << cfg_.direct.serial.port << std::endl;
        return;
    }

    struct termios tty{};
    tcgetattr(serialFd_, &tty);

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    tcsetattr(serialFd_, TCSANOW, &tty);
}

void SolenoidValvePLCDevice::printHex(const uint8_t* data, int len) {
    for (int i = 0; i < len; i++)
        printf("%02X ", data[i]);
    printf("\n");
}

// CRC
uint16_t SolenoidValvePLCDevice::crc16(const uint8_t* data, int len) {
    uint16_t crc = 0xFFFF;

    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
    return crc;
}