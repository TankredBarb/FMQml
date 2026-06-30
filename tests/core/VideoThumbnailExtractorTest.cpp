#include "VideoThumbnailExtractor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cstdio>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool commandExists(const QString &command)
{
    return !QStandardPaths::findExecutable(command).isEmpty();
}

bool generateFixture(const QString &path)
{
    QProcess ffmpeg;
    ffmpeg.setProgram(QStringLiteral("ffmpeg"));
    ffmpeg.setArguments({
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"),
        QStringLiteral("error"),
        QStringLiteral("-y"),
        QStringLiteral("-f"),
        QStringLiteral("lavfi"),
        QStringLiteral("-i"),
        QStringLiteral("testsrc=size=160x90:rate=10:duration=1"),
        QStringLiteral("-pix_fmt"),
        QStringLiteral("yuv420p"),
        path,
    });
    ffmpeg.start();
    if (!ffmpeg.waitForFinished(10000)) {
        ffmpeg.kill();
        ffmpeg.waitForFinished();
        return false;
    }
    return ffmpeg.exitStatus() == QProcess::NormalExit && ffmpeg.exitCode() == 0 && QFile::exists(path);
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (!expect(VideoThumbnailExtractor::isAvailable(), "FFmpeg extractor should report available in gated test")) {
        return 1;
    }

    const VideoThumbnailResult empty = VideoThumbnailExtractor::extract({ {}, QSize(64, 64), -1 });
    if (!expect(empty.image.isNull(), "Empty path should not produce an image")
        || !expect(!empty.error.isEmpty(), "Empty path should report an error")) {
        return 1;
    }

    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory was not created")) {
        return 1;
    }

    const QString invalidPath = QDir(tempDir.path()).filePath(QStringLiteral("not-video.mp4"));
    QFile invalidFile(invalidPath);
    if (!expect(invalidFile.open(QIODevice::WriteOnly), "Invalid fixture could not be created")) {
        return 1;
    }
    invalidFile.write("not a video");
    invalidFile.close();

    const VideoThumbnailResult invalid = VideoThumbnailExtractor::extract({ invalidPath, QSize(64, 64), -1 });
    if (!expect(invalid.image.isNull(), "Invalid video should not produce an image")
        || !expect(!invalid.error.isEmpty(), "Invalid video should report an error")) {
        return 1;
    }

    if (!commandExists(QStringLiteral("ffmpeg"))) {
        std::fprintf(stderr, "ffmpeg executable not found; skipping generated decode fixture\n");
        return 0;
    }

    const QString fixturePath = QDir(tempDir.path()).filePath(QStringLiteral("fixture.mp4"));
    if (!generateFixture(fixturePath)) {
        std::fprintf(stderr, "Could not generate video fixture; skipping decode assertion\n");
        return 0;
    }

    const VideoThumbnailResult decoded = VideoThumbnailExtractor::extract({ fixturePath, QSize(96, 96), -1 });
    if (!expect(decoded.error.isEmpty(), qPrintable(QStringLiteral("Decode failed: %1").arg(decoded.error)))
        || !expect(!decoded.image.isNull(), "Generated fixture should produce an image")
        || !expect(decoded.image.width() <= 96 && decoded.image.height() <= 96, "Decoded thumbnail exceeds target bounds")
        || !expect(decoded.timestampMs >= 0, "Decoded result should report timestamp")
        || !expect(decoded.sourceSize == QSize(160, 90), "Decoded result should report source dimensions")
        || !expect(decoded.streamIndex >= 0, "Decoded result should report stream index")) {
        return 1;
    }

    return 0;
}
