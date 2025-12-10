#ifndef VIDEO_STORAGE_H
#define VIDEO_STORAGE_H

#include <string>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

class VideoStorage {
public:
    VideoStorage(const std::string& output_path, const std::string& format_name);
    ~VideoStorage();

    bool initStorage(AVFormatContext* input_format_ctx);
    
    // Remux模式：直接写packet（推荐）
    bool writePacket(AVPacket* packet);
    
    // 废弃：不应在remux模式使用
    bool writeFrame(AVFrame* frame);
    
    AVFormatContext* getFormatContext() const { return pOutputCtx_; }
    void closeStorage();

private:
    void cleanupOutput();

    std::string outputPath_;
    std::string formatName_;
    AVFormatContext* pOutputCtx_;
    AVStream* pOutputVideoStream_;
    AVCodecContext* pOutputCodecCtx_;  // remux模式不使用，但保留
    AVPacket* pOutputPacket_;
    int64_t startTime_;
    int64_t frameCount_;
    AVRational inputTimeBase_;
};

#endif // VIDEO_STORAGE_H