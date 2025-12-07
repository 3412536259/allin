#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include<string>
#include "logger.h"
extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

class VideoCapture{
public:
    VideoCapture(const std::string& rtsp_url);
    ~VideoCapture();

    bool openStream();
    bool readFrame(AVFrame* frame);
    bool readPacket(AVPacket* packet);
    bool decodePacket(const AVPacket* packet,AVFrame* frame);
    void closeStream();
    // 获取视频流信息（如分辨率、帧率等）
    AVCodecContext* getCodecContext() const { return pCodecCtx_; }
    AVFormatContext* getFormatContext() const { return pFormatCtx_; }

private:
    std::string rtspUrl_;
    AVFormatContext* pFormatCtx_;
    AVCodecContext* pCodecCtx_;
    AVCodec* pCode_;
    int videoStreamIndex_;
    AVPacket* pPacket_;
};

#endif