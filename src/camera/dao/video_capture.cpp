#include "video_capture.h"
#include "logger.h"
#include<iostream>
VideoCapture::VideoCapture(const std::string& rtsp_url)
    :rtspUrl_(rtsp_url),pFormatCtx_(NULL),pCodecCtx_(NULL),
    pCode_(NULL),videoStreamIndex_(-1),pPacket_(NULL)
{
  
    // 初始化网络模块（必须，否则无法拉取网络流）
    avformat_network_init();
    pPacket_ = av_packet_alloc();
    if(!pPacket_){
        LOG_ERROR("Failed to allocate AVPacket");
    }
}

VideoCapture::~VideoCapture(){
    closeStream();
    //释放数据包内存
    if(pPacket_){
        av_packet_free(&pPacket_);
    }
    avformat_network_deinit(); //清理FFmpeg网络模块
}

bool VideoCapture::openStream(){
    //1.分配个上下文
    pFormatCtx_ = avformat_alloc_context();
    if(!pFormatCtx_){
        LOG_ERROR("Failed to allocate AVFormatContext");
        return false;
    }

    LOG_INFO("Opening RTSP stream: " + rtspUrl_);
    int ret = avformat_open_input(&pFormatCtx_,rtspUrl_.c_str(),NULL,NULL);
    if(ret < 0){
        char err_buf[1024] = {0};
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR("Failed to open RTSP stream: " + std::string(err_buf));
        avformat_free_context(pFormatCtx_);
        pFormatCtx_ = NULL;
        return false;
    }

    //获取流信息
    ret = avformat_find_stream_info(pFormatCtx_,NULL);
    if(ret < 0){
        char err_buf[1024] = {0};
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR("Failed to find stream info: " + std::string(err_buf));
        avformat_close_input(&pFormatCtx_);
        avformat_free_context(pFormatCtx_);
        pFormatCtx_ = NULL;
        return false;
    }

    //查找视频流索引
    videoStreamIndex_ = av_find_best_stream(pFormatCtx_,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
    if(videoStreamIndex_ < 0){
        LOG_ERROR("Failed to find video stream in input file");
        avformat_close_input(&pFormatCtx_);
        avformat_free_context(pFormatCtx_);
        pFormatCtx_ = NULL;
        return false;
    }

    //获取视频流编码器
    AVStream* pVideoStream = pFormatCtx_->streams[videoStreamIndex_];
    pCode_ = avcodec_find_decoder(pVideoStream->codecpar->codec_id);
    if(!pCode_){
        LOG_ERROR("Failed to find decoder for video stream");
        avformat_close_input(&pFormatCtx_);
        avformat_free_context(pFormatCtx_);
        pFormatCtx_ = NULL;
        return false;
    }

    //分配编码器上下文并复制参数
    pCodecCtx_ = avcodec_alloc_context3(pCode_);
    if(!pCodecCtx_){
        LOG_ERROR("Failed to allocate decoder context");
        avformat_close_input(&pFormatCtx_);
        avformat_free_context(pFormatCtx_);
        pFormatCtx_ = NULL;
        return false;
    }

    ret = avcodec_parameters_to_context(pCodecCtx_,pVideoStream->codecpar);
    if(ret < 0){
        char err_buf[1024] = {0};
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR("Failed to copy codec parameters to decoder context: " + std::string(err_buf));
        avcodec_free_context(&pCodecCtx_);
        avformat_close_input(&pFormatCtx_);
        avformat_free_context(pFormatCtx_);
        pFormatCtx_ = NULL;
        return false;
    }

    //打开编码器
    ret = avcodec_open2(pCodecCtx_,pCode_,NULL);
    if(ret < 0){
        char err_buf[1024] = {0};
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR("Failed to open decoder: " + std::string(err_buf));
        avcodec_free_context(&pCodecCtx_);
        avformat_close_input(&pFormatCtx_);
        avformat_free_context(pFormatCtx_);
        pFormatCtx_ = NULL;
        return false;
    }

   LOG_INFO("RTSP stream opened successfully. Video stream index: " + std::to_string(videoStreamIndex_));
    return true;
}
bool VideoCapture::readPacket(AVPacket* packet) {
    if (!pFormatCtx_ || !packet) {
        LOG_ERROR("Invalid pointer in readPacket function");
        return false;
    }

    // 重置packet
    av_packet_unref(packet);

    while (true) {
        int ret = av_read_frame(pFormatCtx_, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                LOG_INFO("End of video stream reached");
            } else {
                char err_buf[1024] = {0};
                av_strerror(ret, err_buf, sizeof(err_buf));
                LOG_ERROR("Failed to read packet: " + std::string(err_buf));
            }
            return false;
        }

        // 只返回视频流的packet
        if (packet->stream_index == videoStreamIndex_) {
            return true;
        }

        // 不是视频流（可能是音频流），释放并继续读取
        av_packet_unref(packet);
    }

    return false;
}
bool VideoCapture::decodePacket(const AVPacket* packet,AVFrame* frame){
    if(!pCodecCtx_ || !packet || !frame){
        LOG_ERROR("Invalid pointer in decodePacket function");
        return false;
    }
    // 发送数据包到解码器
    int ret = avcodec_send_packet(pCodecCtx_, packet);
    if (ret < 0) {
        char err_buf[1024] = {0};
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR("Failed to send packet to decoder: " + std::string(err_buf));
        // av_packet_unref(pPacket_);
        return false;
    }

    // 接收解码后的帧
    ret = avcodec_receive_frame(pCodecCtx_, frame);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return readFrame(frame); // 需继续读取数据包或已到流末尾
        } else {
            char err_buf[1024] = {0};
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_ERROR("Failed to receive frame from decoder: " + std::string(err_buf));
            return false;
        }
    }

    return true;

}


bool VideoCapture::readFrame(AVFrame* frame){
    if( !pFormatCtx_ || !pCodecCtx_ || !pPacket_ || !frame){
        LOG_ERROR("Invalid pointer in readFrame function");
        return false;
    }

    //重置帧数据
    av_frame_unref(frame);

    int ret = av_read_frame(pFormatCtx_,pPacket_);
    if(ret < 0){
        if(ret == AVERROR_EOF){
            LOG_INFO("End of video stream reached");
        }else{
            char err_buf[1024] = {0};
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_ERROR("Failed to read frame: " + std::string(err_buf));
        }
        return false;
    }

    // 判断是否为视频流数据包
    if (pPacket_->stream_index != videoStreamIndex_) {
        av_packet_unref(pPacket_);
        return readFrame(frame); // 递归读取下一个数据包
    }

    // 发送数据包到解码器
    ret = avcodec_send_packet(pCodecCtx_, pPacket_);
    if (ret < 0) {
        char err_buf[1024] = {0};
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR("Failed to send packet to decoder: " + std::string(err_buf));
        av_packet_unref(pPacket_);
        return false;
    }

    // 接收解码后的帧
    ret = avcodec_receive_frame(pCodecCtx_, frame);
    av_packet_unref(pPacket_); // 释放数据包引用
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return readFrame(frame); // 需继续读取数据包或已到流末尾
        } else {
            char err_buf[1024] = {0};
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_ERROR("Failed to receive frame from decoder: " + std::string(err_buf));
            return false;
        }
    }

    return true;
}

void VideoCapture::closeStream(){
        // 关闭编码器
    if (pCodecCtx_) {
        avcodec_close(pCodecCtx_);
        avcodec_free_context(&pCodecCtx_);
        pCodecCtx_ = NULL;
    }

    // 关闭输入流
    if (pFormatCtx_) {
        avformat_close_input(&pFormatCtx_);
        avformat_free_context(pFormatCtx_);
        pFormatCtx_ = NULL;
    }

    videoStreamIndex_ = -1;
    pCode_ = NULL;
    LOG_INFO("RTSP stream closed successfully");
}