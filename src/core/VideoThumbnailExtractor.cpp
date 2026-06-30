#include "VideoThumbnailExtractor.h"

#include <QByteArray>
#include <QFile>
#include <QtGlobal>

#include <algorithm>
#include <memory>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace {
constexpr int kMaxPacketsAfterSeek = 180;
constexpr int kMaxFramesAfterSeek = 60;
constexpr int kMaxSourceDimension = 16384;

void configureAvLogging()
{
    static std::once_flag once;
    std::call_once(once, []() {
        av_log_set_level(AV_LOG_ERROR);
    });
}

struct FormatContextDeleter {
    void operator()(AVFormatContext *context) const
    {
        if (context) {
            avformat_close_input(&context);
        }
    }
};

struct CodecContextDeleter {
    void operator()(AVCodecContext *context) const
    {
        avcodec_free_context(&context);
    }
};

struct PacketDeleter {
    void operator()(AVPacket *packet) const
    {
        av_packet_free(&packet);
    }
};

struct FrameDeleter {
    void operator()(AVFrame *frame) const
    {
        av_frame_free(&frame);
    }
};

struct SwsContextDeleter {
    void operator()(SwsContext *context) const
    {
        sws_freeContext(context);
    }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

QString avErrorString(int errorCode)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

qint64 selectTimestampMs(const AVFormatContext *formatContext, const AVStream *stream, qint64 preferredTimestampMs)
{
    if (preferredTimestampMs >= 0) {
        return preferredTimestampMs;
    }

    qint64 durationMs = -1;
    if (stream && stream->duration > 0) {
        durationMs = av_rescale_q(stream->duration, stream->time_base, AVRational{1, 1000});
    }
    if (durationMs <= 0 && formatContext && formatContext->duration > 0) {
        durationMs = formatContext->duration / (AV_TIME_BASE / 1000);
    }
    if (durationMs <= 0) {
        return 1000;
    }

    return std::clamp<qint64>(durationMs / 10, 500, 30000);
}

bool validSourceDimensions(const AVCodecContext *codecContext)
{
    return codecContext
        && codecContext->width > 0
        && codecContext->height > 0
        && codecContext->width <= kMaxSourceDimension
        && codecContext->height <= kMaxSourceDimension;
}

QImage frameToImage(const AVFrame *frame, const QSize &targetSize)
{
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        return {};
    }

    QImage image(frame->width, frame->height, QImage::Format_RGB32);
    if (image.isNull()) {
        return {};
    }

    SwsContextPtr swsContext(sws_getContext(
        frame->width,
        frame->height,
        static_cast<AVPixelFormat>(frame->format),
        image.width(),
        image.height(),
        AV_PIX_FMT_BGRA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr));
    if (!swsContext) {
        return {};
    }

    uint8_t *destinationData[] = { image.bits(), nullptr, nullptr, nullptr };
    const int destinationLinesize[] = { static_cast<int>(image.bytesPerLine()), 0, 0, 0 };
    const int convertedRows = sws_scale(
        swsContext.get(),
        frame->data,
        frame->linesize,
        0,
        frame->height,
        destinationData,
        destinationLinesize);
    if (convertedRows <= 0) {
        return {};
    }

    const QSize scaledTarget = targetSize.isValid() ? targetSize : QSize(128, 128);
    return image.scaled(scaledTarget, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
} // namespace

bool VideoThumbnailExtractor::isAvailable()
{
    return true;
}

VideoThumbnailResult VideoThumbnailExtractor::extract(const VideoThumbnailRequest &request)
{
    configureAvLogging();

    VideoThumbnailResult result;
    if (request.path.isEmpty()) {
        result.error = QStringLiteral("empty path");
        return result;
    }

    AVFormatContext *rawFormatContext = nullptr;
    const QByteArray pathBytes = QFile::encodeName(request.path);
    int status = avformat_open_input(&rawFormatContext, pathBytes.constData(), nullptr, nullptr);
    FormatContextPtr formatContext(rawFormatContext);
    if (status < 0) {
        result.error = QStringLiteral("open failed: %1").arg(avErrorString(status));
        return result;
    }

    status = avformat_find_stream_info(formatContext.get(), nullptr);
    if (status < 0) {
        result.error = QStringLiteral("stream info failed: %1").arg(avErrorString(status));
        return result;
    }

    status = av_find_best_stream(formatContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (status < 0) {
        result.error = QStringLiteral("video stream not found: %1").arg(avErrorString(status));
        return result;
    }

    const int streamIndex = status;
    AVStream *stream = formatContext->streams[streamIndex];
    result.streamIndex = streamIndex;
    result.sourceSize = QSize(stream->codecpar->width, stream->codecpar->height);
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        result.error = QStringLiteral("decoder not found");
        return result;
    }

    CodecContextPtr codecContext(avcodec_alloc_context3(codec));
    if (!codecContext) {
        result.error = QStringLiteral("decoder context allocation failed");
        return result;
    }

    status = avcodec_parameters_to_context(codecContext.get(), stream->codecpar);
    if (status < 0) {
        result.error = QStringLiteral("decoder parameters failed: %1").arg(avErrorString(status));
        return result;
    }
    if (!validSourceDimensions(codecContext.get())) {
        result.error = QStringLiteral("invalid source dimensions");
        return result;
    }

    status = avcodec_open2(codecContext.get(), codec, nullptr);
    if (status < 0) {
        result.error = QStringLiteral("decoder open failed: %1").arg(avErrorString(status));
        return result;
    }

    result.timestampMs = selectTimestampMs(formatContext.get(), stream, request.preferredTimestampMs);
    const qint64 streamTimestamp = av_rescale_q(result.timestampMs, AVRational{1, 1000}, stream->time_base);
    av_seek_frame(formatContext.get(), streamIndex, streamTimestamp, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecContext.get());

    PacketPtr packet(av_packet_alloc());
    FramePtr frame(av_frame_alloc());
    if (!packet || !frame) {
        result.error = QStringLiteral("frame allocation failed");
        return result;
    }

    int packetsRead = 0;
    int framesDecoded = 0;
    while (packetsRead < kMaxPacketsAfterSeek && framesDecoded < kMaxFramesAfterSeek) {
        status = av_read_frame(formatContext.get(), packet.get());
        if (status < 0) {
            break;
        }

        ++packetsRead;
        if (packet->stream_index != streamIndex) {
            av_packet_unref(packet.get());
            continue;
        }

        status = avcodec_send_packet(codecContext.get(), packet.get());
        av_packet_unref(packet.get());
        if (status < 0 && status != AVERROR(EAGAIN)) {
            continue;
        }

        while (framesDecoded < kMaxFramesAfterSeek) {
            status = avcodec_receive_frame(codecContext.get(), frame.get());
            if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) {
                break;
            }
            if (status < 0) {
                result.error = QStringLiteral("decode failed: %1").arg(avErrorString(status));
                return result;
            }

            ++framesDecoded;
            result.image = frameToImage(frame.get(), request.targetSize);
            av_frame_unref(frame.get());
            if (!result.image.isNull()) {
                return result;
            }
        }
    }

    result.error = QStringLiteral("no decodable video frame");
    return result;
}
