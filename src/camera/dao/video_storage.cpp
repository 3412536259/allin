#include "video_storage.h"
#include "logger.h"
#include <cstring>

VideoStorage::VideoStorage(const std::string& output_path, const std::string& format_name)
    : outputPath_(output_path),
      formatName_(format_name),
      pOutputCtx_(nullptr),
      pOutputVideoStream_(nullptr),
      pOutputCodecCtx_(nullptr),
      pOutputPacket_(nullptr),
      startTime_(0),
      frameCount_(0),
      inputTimeBase_({0, 0}) {

    pOutputPacket_ = av_packet_alloc();
    if (!pOutputPacket_) {
        LOG_ERROR("Failed to allocate AVPacket");
    }
}

VideoStorage::~VideoStorage() {
    closeStorage();
    if (pOutputPacket_) {
        av_packet_free(&pOutputPacket_);
    }
}

bool VideoStorage::initStorage(AVFormatContext* input_format_ctx) {
    if (!input_format_ctx || !pOutputPacket_) {
        LOG_ERROR("Invalid pointer passed to initStorage()");
        return false;
    }

    frameCount_ = 0;

    // 1. 查找输出格式
    AVOutputFormat* pOutputFormat = av_guess_format(formatName_.c_str(), outputPath_.c_str(), nullptr);
    if (!pOutputFormat) {
        LOG_ERROR("Failed to guess output format: " + formatName_);
        return false;
    }

    // 2. 分配输出格式上下文
    int ret = avformat_alloc_output_context2(&pOutputCtx_, pOutputFormat, nullptr, outputPath_.c_str());
    if (!pOutputCtx_) {
        LOG_ERROR("Failed to allocate output format context");
        return false;
    }

    // 3. 找到输入视频流
    int inputVideoStreamIndex = av_find_best_stream(input_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (inputVideoStreamIndex < 0) {
        LOG_ERROR("Failed to find input video stream");
        cleanupOutput();
        return false;
    }

    AVStream* pInputVideoStream = input_format_ctx->streams[inputVideoStreamIndex];
    AVCodecParameters* in_codecpar = pInputVideoStream->codecpar;
    inputTimeBase_ = pInputVideoStream->time_base;

    // 打印输入流信息
    LOG_INFO("Input codec: " + std::string(avcodec_get_name(in_codecpar->codec_id)));
    LOG_INFO("Input time_base: " + std::to_string(inputTimeBase_.num) + "/" + std::to_string(inputTimeBase_.den));
    
    AVRational frame_rate = av_guess_frame_rate(input_format_ctx, pInputVideoStream, nullptr);
    if (frame_rate.num > 0 && frame_rate.den > 0) {
        LOG_INFO("Frame rate: " + std::to_string(frame_rate.num) + "/" + std::to_string(frame_rate.den) + 
                 " ≈ " + std::to_string((double)frame_rate.num / frame_rate.den) + " fps");
    }

    // 4. 创建输出流 - 直接复制编码参数（remux模式，不重新编码）
    pOutputVideoStream_ = avformat_new_stream(pOutputCtx_, nullptr);
    if (!pOutputVideoStream_) {
        LOG_ERROR("Failed to create new output stream");
        cleanupOutput();
        return false;
    }

    // 直接复制编码参数
    ret = avcodec_parameters_copy(pOutputVideoStream_->codecpar, in_codecpar);
    if (ret < 0) {
        LOG_ERROR("Failed to copy codec parameters");
        cleanupOutput();
        return false;
    }

    // 设置输出流时间基
    pOutputVideoStream_->time_base = inputTimeBase_;
    
    // codec_tag 设为0，让muxer自动选择
    pOutputVideoStream_->codecpar->codec_tag = 0;

    // 5. 打开输出文件
    if (!(pOutputFormat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&pOutputCtx_->pb, outputPath_.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char err_buf[128];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_ERROR(std::string("Failed to open output file: ") + err_buf);
            cleanupOutput();
            return false;
        }
    }

    // 6. 写文件头
    ret = avformat_write_header(pOutputCtx_, nullptr);
    if (ret < 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR(std::string("Failed to write file header: ") + err_buf);
        cleanupOutput();
        return false;
    }

    startTime_ = av_gettime();
    LOG_INFO("Video storage initialized (REMUX mode - no re-encoding): " + outputPath_);
    return true;
}

bool VideoStorage::writeFrame(AVFrame* frame) {
    if (!pOutputCodecCtx_ || !pOutputPacket_ || !pOutputCtx_ || !pOutputVideoStream_ || !frame) {
        LOG_ERROR("Invalid pointer in writeFrame()");
        return false;
    }

    static int64_t frame_index = 0;
    frame->pts = frame_index++;

    int ret = avcodec_send_frame(pOutputCodecCtx_, frame);
    if (ret < 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR(std::string("Failed to send frame: ") + err_buf);
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(pOutputCodecCtx_, pOutputPacket_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            char err_buf[128];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_ERROR(std::string("Failed to receive packet: ") + err_buf);
            return false;
        }

        av_packet_rescale_ts(pOutputPacket_, pOutputCodecCtx_->time_base, pOutputVideoStream_->time_base);
        pOutputPacket_->stream_index = pOutputVideoStream_->index;

        ret = av_interleaved_write_frame(pOutputCtx_, pOutputPacket_);
        av_packet_unref(pOutputPacket_);
        if (ret < 0) {
            char err_buf[128];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOG_ERROR(std::string("Failed to write packet: ") + err_buf);
            return false;
        }
    }

    return true;
}

// 新增：直接写packet的方法（remux模式）
bool VideoStorage::writePacket(AVPacket* packet) {
    if (!pOutputCtx_ || !pOutputVideoStream_ || !packet) {
        LOG_ERROR("Invalid pointer in writePacket()");
        return false;
    }

    // 复制packet以避免修改原始数据
    AVPacket* pkt = av_packet_clone(packet);
    if (!pkt) {
        LOG_ERROR("Failed to clone packet");
        return false;
    }

    // 转换时间戳：从输入流时间基转到输出流时间基
    av_packet_rescale_ts(pkt, inputTimeBase_, pOutputVideoStream_->time_base);
    pkt->stream_index = pOutputVideoStream_->index;

    // 写入packet
    int ret = av_interleaved_write_frame(pOutputCtx_, pkt);
    av_packet_free(&pkt);
    
    if (ret < 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOG_ERROR(std::string("Failed to write packet: ") + err_buf);
        return false;
    }

    frameCount_++;
    return true;
}

void VideoStorage::closeStorage() {
    if (!pOutputCtx_)
        return;

    // 写入文件尾
    av_write_trailer(pOutputCtx_);

    // 关闭文件和释放资源
    cleanupOutput();

    if (pOutputCodecCtx_) {
        avcodec_free_context(&pOutputCodecCtx_);
        pOutputCodecCtx_ = nullptr;
    }

    pOutputVideoStream_ = nullptr;
    
    LOG_INFO("Video storage closed. Total packets: " + std::to_string(frameCount_) + ", file: " + outputPath_);
    
    startTime_ = 0;
    frameCount_ = 0;
}

void VideoStorage::cleanupOutput() {
    if (pOutputCtx_) {
        if (!(pOutputCtx_->oformat->flags & AVFMT_NOFILE) && pOutputCtx_->pb) {
            avio_closep(&pOutputCtx_->pb);
        }
        avformat_free_context(pOutputCtx_);
        pOutputCtx_ = nullptr;
    }
}