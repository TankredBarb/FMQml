#include "SvgRecolorProvider.h"

#include <QColor>
#include <QFile>
#include <QMutexLocker>
#include <QPainter>
#include <QPointF>
#include <QRegularExpression>
#include <QRectF>
#include <QSizeF>
#include <QStringList>
#include <QSvgRenderer>
#include <QUrl>

SvgRecolorProvider::SvgRecolorProvider()
    : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_cache(600)
{
}

SvgRecolorProvider::~SvgRecolorProvider() = default;

namespace {
QString normalizeResourcePath(QString source)
{
    if (source.startsWith(QStringLiteral("qrc:/"))) {
        source.remove(0, 3);
        return source;
    }

    if (source.startsWith(QStringLiteral(":/"))) {
        return source;
    }

    const int assetsIndex = source.indexOf(QStringLiteral("assets/"));
    if (assetsIndex >= 0) {
        return QStringLiteral(":/qt/qml/FM/qml/") + source.mid(assetsIndex);
    }

    return source;
}

void recolorXmlAttribute(QString &svg, const QString &attribute, const QString &color)
{
    const QRegularExpression attributePattern(
        QStringLiteral(R"((\b%1\s*=\s*["'])(?!none\b|transparent\b|url\()([^"']+)(["']))").arg(attribute),
        QRegularExpression::CaseInsensitiveOption);
    svg.replace(attributePattern, QStringLiteral("\\1%1\\3").arg(color));

    const QRegularExpression stylePattern(
        QStringLiteral(R"((%1\s*:\s*)(?!none\b|transparent\b|url\()([^;"']+))").arg(attribute),
        QRegularExpression::CaseInsensitiveOption);
    svg.replace(stylePattern, QStringLiteral("\\1%1").arg(color));
}

QStringList protectSvgBlocks(QString &svg)
{
    QStringList protectedBlocks;
    const QRegularExpression blockPattern(
        QStringLiteral(R"(<(mask|clipPath)\b[^>]*>.*?</\1>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);

    qsizetype offset = 0;
    while (true) {
        const QRegularExpressionMatch match = blockPattern.match(svg, offset);
        if (!match.hasMatch()) {
            break;
        }

        const QString placeholder = QStringLiteral("__FM_SVG_RECOLOR_PROTECTED_%1__").arg(protectedBlocks.size());
        protectedBlocks.append(match.captured(0));
        svg.replace(match.capturedStart(0), match.capturedLength(0), placeholder);
        offset = match.capturedStart(0) + placeholder.size();
    }

    return protectedBlocks;
}

void restoreSvgBlocks(QString &svg, const QStringList &protectedBlocks)
{
    for (qsizetype i = 0; i < protectedBlocks.size(); ++i) {
        svg.replace(QStringLiteral("__FM_SVG_RECOLOR_PROTECTED_%1__").arg(i), protectedBlocks.at(i));
    }
}

QSize targetSizeFor(const QSvgRenderer &renderer, const QSize &requestedSize)
{
    if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
        return requestedSize;
    }

    QSize size = renderer.defaultSize();
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
        size = QSize(24, 24);
    }
    return size;
}
}

QImage SvgRecolorProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString payload = QUrl::fromPercentEncoding(id.toUtf8());
    const QString cacheKey = payload
        + QStringLiteral("|")
        + QString::number(requestedSize.width())
        + QStringLiteral("x")
        + QString::number(requestedSize.height());

    {
        QMutexLocker locker(&m_mutex);
        if (m_cache.contains(cacheKey)) {
            QImage cached = *m_cache.object(cacheKey);
            if (size) {
                *size = cached.size();
            }
            return cached;
        }
    }

    QImage image = renderPayload(payload, requestedSize);
    if (size) {
        *size = image.size();
    }

    {
        QMutexLocker locker(&m_mutex);
        if (!m_cache.contains(cacheKey)) {
            m_cache.insert(cacheKey, new QImage(image));
        }
    }

    return image;
}

QImage SvgRecolorProvider::renderPayload(const QString &payload, const QSize &requestedSize)
{
    const QStringList parts = payload.split(QLatin1Char('\n'));
    if (parts.size() < 2) {
        return {};
    }

    const QString sourcePath = normalizeResourcePath(parts.at(0));
    const QColor color(parts.at(1));
    const bool recolorStroke = parts.value(2, QStringLiteral("1")) == QLatin1String("1");
    const bool recolorFill = parts.value(3, QStringLiteral("1")) == QLatin1String("1");

    if (!color.isValid()) {
        return {};
    }

    QFile file(sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QString svg = QString::fromUtf8(file.readAll());
    const QString colorName = color.name(QColor::HexRgb);
    const QStringList protectedBlocks = protectSvgBlocks(svg);
    if (recolorStroke) {
        recolorXmlAttribute(svg, QStringLiteral("stroke"), colorName);
    }
    if (recolorFill) {
        recolorXmlAttribute(svg, QStringLiteral("fill"), colorName);
    }
    restoreSvgBlocks(svg, protectedBlocks);

    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        return {};
    }

    const QSize targetSize = targetSizeFor(renderer, requestedSize);
    QImage image(targetSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(targetSize)));

    return image;
}
