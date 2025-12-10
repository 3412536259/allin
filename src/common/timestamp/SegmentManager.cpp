#include "SegmentManager.h"
#include "FileUtils.h"
#include "logger.h"

SegmentManager::SegmentManager(const std::string& dir, const std::string& format, int durationSec)
    : dir_(dir), format_(format), durationSec_(durationSec), segmentIndex_(0) {
    lastStart_ = std::chrono::steady_clock::now();
}

bool SegmentManager::needNewSegment() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastStart_).count();
    return elapsed >= durationSec_;
}

std::unique_ptr<VideoStorage> SegmentManager::createSegment(AVFormatContext* inputCtx) {
    std::string outputFile = FileUtils::generateFileName(dir_,format_);
    auto storage = std::make_unique<VideoStorage>(outputFile, format_);
    if (!storage->initStorage(inputCtx)) {
        LOG_ERROR("Failed to init new segment storage");
        return nullptr;
    }

    lastStart_ = std::chrono::steady_clock::now();
    segmentIndex_++;

    LOG_INFO("Started new segment #" + std::to_string(segmentIndex_) + ": " + outputFile);
    return storage;
}