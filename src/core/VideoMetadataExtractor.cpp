#include "VideoMetadataExtractor.h"

#include <QFile>
#include <QVariantMap>

#include <algorithm>
#include <memory>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

namespace {

struct FormatContextDeleter {
    void operator()(AVFormatContext *context) const
    {
        if (context) {
            avformat_close_input(&context);
        }
    }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

void configureAvLogging()
{
    static std::once_flag once;
    std::call_once(once, []() {
        av_log_set_level(AV_LOG_ERROR);
    });
}

void add(QVariantList &list, const QString &label, const QString &value)
{
    if (!value.isEmpty()) {
        list.append(QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("value"), value}});
    }
}

QString codecName(AVCodecID codecId)
{
    const char *name = avcodec_get_name(codecId);
    return name ? QString::fromUtf8(name).toUpper() : QString();
}

QString pixelFormatName(int format)
{
    const char *name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(format));
    return name ? QString::fromUtf8(name) : QString();
}

QString durationText(qint64 milliseconds)
{
    if (milliseconds <= 0) {
        return {};
    }

    const qint64 totalSeconds = milliseconds / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString frameRateText(const AVRational &rate)
{
    if (rate.num <= 0 || rate.den <= 0) {
        return {};
    }

    const double fps = av_q2d(rate);
    if (fps <= 0.0) {
        return {};
    }
    return QStringLiteral("%1 fps").arg(fps, 0, 'f', fps >= 100.0 ? 1 : 2);
}

QString bitrateText(qint64 bitrate)
{
    if (bitrate <= 0) {
        return {};
    }

    return QStringLiteral("%1 kbps").arg(std::max<qint64>(1, bitrate / 1000));
}

QString audioChannelsText(int channels)
{
    if (channels <= 0) {
        return {};
    }
    if (channels == 1) {
        return QStringLiteral("Mono");
    }
    if (channels == 2) {
        return QStringLiteral("Stereo");
    }
    return QStringLiteral("%1 channels").arg(channels);
}

qint64 streamDurationMs(const AVFormatContext *formatContext, const AVStream *stream)
{
    if (stream && stream->duration > 0) {
        return av_rescale_q(stream->duration, stream->time_base, AVRational{1, 1000});
    }
    if (formatContext && formatContext->duration > 0) {
        return formatContext->duration / (AV_TIME_BASE / 1000);
    }
    return -1;
}

} // namespace

QVariantList VideoMetadataExtractor::extract(const QString &path)
{
    QVariantList props;
    if (path.isEmpty()) {
        return props;
    }

    configureAvLogging();

    AVFormatContext *rawFormatContext = nullptr;
    const QByteArray pathBytes = QFile::encodeName(path);
    int status = avformat_open_input(&rawFormatContext, pathBytes.constData(), nullptr, nullptr);
    FormatContextPtr formatContext(rawFormatContext);
    if (status < 0) {
        return props;
    }

    status = avformat_find_stream_info(formatContext.get(), nullptr);
    if (status < 0) {
        return props;
    }

    const int videoStreamIndex = av_find_best_stream(formatContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        return props;
    }

    AVStream *videoStream = formatContext->streams[videoStreamIndex];
    const AVCodecParameters *videoParams = videoStream->codecpar;
    if (!videoParams || videoParams->width <= 0 || videoParams->height <= 0) {
        return props;
    }

    add(props, QStringLiteral("Format"), QString::fromUtf8(formatContext->iformat ? formatContext->iformat->long_name : ""));
    add(props, QStringLiteral("Duration"), durationText(streamDurationMs(formatContext.get(), videoStream)));
    add(props, QStringLiteral("Dimensions"), QStringLiteral("%1 × %2").arg(videoParams->width).arg(videoParams->height));
    add(props, QStringLiteral("Video Codec"), codecName(videoParams->codec_id));
    add(props, QStringLiteral("Frame Rate"), frameRateText(av_guess_frame_rate(formatContext.get(), videoStream, nullptr)));
    add(props, QStringLiteral("Bitrate"), bitrateText(videoParams->bit_rate > 0 ? videoParams->bit_rate : formatContext->bit_rate));
    add(props, QStringLiteral("Pixel Format"), pixelFormatName(videoParams->format));

    const int audioStreamIndex = av_find_best_stream(formatContext.get(), AVMEDIA_TYPE_AUDIO, -1, videoStreamIndex, nullptr, 0);
    if (audioStreamIndex >= 0) {
        AVStream *audioStream = formatContext->streams[audioStreamIndex];
        const AVCodecParameters *audioParams = audioStream->codecpar;
        if (audioParams) {
            add(props, QStringLiteral("Audio Codec"), codecName(audioParams->codec_id));
            if (audioParams->sample_rate > 0) {
                add(props, QStringLiteral("Sample Rate"),
                    QStringLiteral("%1 kHz").arg(static_cast<double>(audioParams->sample_rate) / 1000.0, 0, 'f', 1));
            }
            add(props, QStringLiteral("Channels"), audioChannelsText(audioParams->ch_layout.nb_channels));
        }
    }

    return props;
}
