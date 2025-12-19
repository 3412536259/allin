#include "SegmentManager.h"
#include "FileUtils.h"
#include "logger.h"
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;
SegmentManager::SegmentManager(const std::string& dir, const std::string& format, int durationSec)
    : dir_(dir), format_(format), durationSec_(durationSec), segmentIndex_(0) {  
    lastStart_ = std::chrono::steady_clock::now();
}

bool SegmentManager::needNewSegment() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastStart_).count();
    return elapsed >= durationSec_;
}

static int toDateInt(std::chrono::system_clock::time_point tp)
{
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm local{};
    localtime_r(&tt, &local);

    return (local.tm_year + 1900) * 10000
         + (local.tm_mon + 1) * 100
         + local.tm_mday;
}

// helper: convert filesystem file_time_type to system_clock::time_point
static std::chrono::system_clock::time_point fileTimeToSystemClock(const fs::file_time_type& ft)
{
    using namespace std::chrono;
    // approximate conversion by offset from fs clock to system clock
    auto now_sys = system_clock::now();
    auto now_fs = fs::file_time_type::clock::now();
    return time_point_cast<system_clock::duration>(now_sys + (ft - now_fs));
}

void SegmentManager::cleanExpiredFiles(int retainDays)
{
    using namespace std::chrono;

    if (!fs::exists(dir_) || !fs::is_directory(dir_)) {
        return;
    }

    auto now = system_clock::now();
    auto expireTp = now - hours(24 * retainDays);
    int expireDate = toDateInt(expireTp);

    for (const auto& entry : fs::directory_iterator(dir_)) {
        if (!entry.is_directory()) continue;

        std::string dateStr = entry.path().filename().string();
        if (dateStr.size() != 8) continue;

        int dirDate = std::stoi(dateStr);

        if (dirDate < expireDate) {
            try {
                fs::remove_all(entry.path());
                LOG_INFO("Deleted expired directory: " + entry.path().string());
            } catch (const std::exception& e) {
                LOG_WARNING("Failed to delete directory: "
                            + entry.path().string()
                            + " error=" + e.what());
            }
        }
    }
}

void SegmentManager::cleanExpiredFilesHours(int retainHours)
{
    using namespace std::chrono;
    if (!fs::exists(dir_) || !fs::is_directory(dir_)) return;

    auto now = system_clock::now();
    auto cutoff = now - hours(retainHours);

    for (const auto& entry : fs::directory_iterator(dir_)) {
        if (!entry.is_directory()) continue;
        // iterate files recursively and delete based on last_write_time
        for (const auto& f : fs::recursive_directory_iterator(entry.path(), fs::directory_options::skip_permission_denied)) {
            try {
                if (!f.is_regular_file()) continue;
                auto ftime = fs::last_write_time(f.path());
                auto ftime_sys = fileTimeToSystemClock(ftime);
                if (ftime_sys < cutoff) {
                    try {
                        fs::remove(f.path());
                        LOG_INFO("Deleted expired file: " + f.path().string());
                    } catch (const std::exception& e) {
                        LOG_WARNING(std::string("Failed to delete file: ") + f.path().string() + " error=" + e.what());
                    }
                }
            } catch (const std::exception& e) {
                LOG_WARNING(std::string("Failed to inspect file: ") + e.what());
            }
        }
        // try to remove empty directories
        try {
            if (fs::is_empty(entry.path())) {
                fs::remove(entry.path());
                LOG_INFO("Removed empty directory: " + entry.path().string());
            }
        } catch (...) {
            // ignore
        }
    }
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