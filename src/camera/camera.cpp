#include "camera.h"
#include "logger.h"
#include "SegmentManager.h"
#include "TimestampAdjuster.h"
#include <iostream>
extern "C" {
#include <libavutil/time.h>
}
const std::string ROOT_DIR = "/home/ztl/workspace/allin/videos/";
const std::string VIDEOFORMAT = "mp4";
const int SEGMENTDURATIONSEC = 60;



Camera::Camera(CameraStaticInfo cameraStaticInfo)
    :cameraStaticInfo_(cameraStaticInfo),videoCapture_(cameraStaticInfo_.rtsp_url)
{
    std::lock_guard<std::mutex> lock(statusMutex_);
    cameraStatus_.online_status = CameraOnlineStatus::OFFLINE;
    cameraStatus_.camera_id = cameraStaticInfo_.camera_id;
}
Camera::~Camera()
{
    stop();
}
bool Camera::start()
{
    if (isRunning_) return true;

    isRunning_ = true;
    pullThread_ = std::thread(&Camera::pullKeyFrameLoop, this);
    return true;
}
bool Camera::stop()
{
    if (!isRunning_) return true;

    isRunning_ = false;
    if (pullThread_.joinable())
        pullThread_.join();

    videoCapture_.closeStream();
    std::lock_guard<std::mutex> lock(statusMutex_);
    cameraStatus_.online_status = CameraOnlineStatus::OFFLINE;
    return true;
}

CameraStatus Camera::getStatus()
{
    std::lock_guard<std::mutex> lock(statusMutex_);
    return cameraStatus_;
}


bool Camera::isRunning() const
{
    return isRunning_;
}

bool Camera::getLastKeyFrame(FrameData& out) {
    std::lock_guard<std::mutex> lock(keyFrameMutex_);
    if (!lastKeyFrame_.frame)
        return false;

    out = lastKeyFrame_;  
    return true;
}


void Camera::pullKeyFrameLoop()
{
    LOG_INFO("=== VideoRecorder Started ===");
    if (!videoCapture_.openStream()) {
        LOG_ERROR("Failed to open RTSP stream");
        isRunning_ = false;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        cameraStatus_.online_status = CameraOnlineStatus::ONLINE;

    }
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Failed to allocate AVPacket");
        videoCapture_.closeStream();
        isRunning_ = false;
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            cameraStatus_.online_status = CameraOnlineStatus::OFFLINE;
        }

        return;
    }
    std::string video_dir = ROOT_DIR+this->cameraStaticInfo_.camera_id;
    SegmentManager segMgr(video_dir, VIDEOFORMAT, SEGMENTDURATIONSEC);
    TimestampAdjuster tsAdjuster;

    auto storage = segMgr.createSegment(videoCapture_.getFormatContext());
    if (!storage) {
        LOG_ERROR("Failed to create first segment");
        av_packet_free(&packet);
        videoCapture_.closeStream();
        isRunning_ = false;
        return;
    }

    // Hourly cleaner settings: retain files older than HOURLY_CLEAN_RETAIN_HOURS and check every HOURLY_CLEAN_INTERVAL_HOURS
    const int HOURLY_CLEAN_RETAIN_HOURS = 4; // 删除 4 小时以前的文件
    const int HOURLY_CLEAN_INTERVAL_HOURS = 1; // 每 1 小时检查一次

    std::thread hourlyCleaner;
    hourlyCleaner = std::thread([&segMgr, this, HOURLY_CLEAN_RETAIN_HOURS, HOURLY_CLEAN_INTERVAL_HOURS]() {
        while (isRunning_) {
            // sleep in small increments so we can exit promptly when stopping
            int sleepSeconds = HOURLY_CLEAN_INTERVAL_HOURS * 3600;
            for (int i = 0; i < sleepSeconds && isRunning_; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (!isRunning_) break;
            segMgr.cleanExpiredFilesHours(HOURLY_CLEAN_RETAIN_HOURS);
        }
    });

    int totalPackets = 0;
    while(isRunning_)
    {
        if (segMgr.needNewSegment()) {
            storage->closeStorage();
            //std::cout << 1 << std::endl;
            segMgr.cleanExpiredFiles(7);
            //std::cout << 2 << std::endl;
            storage = segMgr.createSegment(videoCapture_.getFormatContext());
            tsAdjuster.reset();
            if (!storage) break;
        }

        if (!videoCapture_.readPacket(packet)) {
            LOG_WARNING("Failed to read packet, retrying...");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        tsAdjuster.adjust(packet);

        if (!storage->writePacket(packet)) {
            LOG_WARNING("Failed to write packet");
        } else {
            totalPackets++;
            if (totalPackets % 200 == 0)
                LOG_INFO("Written " + std::to_string(totalPackets) + " packets total");
        }
                //对关键帧
        if (packet->flags & AV_PKT_FLAG_KEY) {
            if (videoCapture_.decodePacket(packet, frame) >= 0) {  
                std::lock_guard<std::mutex> lock(keyFrameMutex_);        
                lastKeyFrame_.frame = std::shared_ptr<AVFrame>(
                av_frame_clone(frame), [](AVFrame* f) { av_frame_free(&f); });
                lastKeyFrame_.timestamp = av_gettime_relative();
            }
        }
       
        av_packet_unref(packet);
    }
    if (storage) storage->closeStorage();

    av_packet_free(&packet);
    av_frame_free(&frame);
    videoCapture_.closeStream();

    // signal cleaner to stop and join thread (if started)
    isRunning_ = false;
    if (hourlyCleaner.joinable()) hourlyCleaner.join();

    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        cameraStatus_.online_status = CameraOnlineStatus::OFFLINE;
    }

    LOG_INFO("=== VideoRecorder Finished ===");
}