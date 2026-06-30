#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMimeType>

// Unified metadata extraction for all file types.
// Returns a QVariantList of QVariantMap { "label": ..., "value": ... } pairs
// suitable for direct use in QML Repeaters.
class MetadataExtractor {
public:
    static QVariantList extract(const QString &path);

private:
    static QVariantList extractAudio(const QString &path);
    static QVariantList extractImage(const QString &path);
    static QVariantList extractText(const QString &path);
    static QVariantList extractSvg(const QString &path);
    static QVariantList extractFont(const QString &path);
    static QVariantList extractPdf(const QString &path);
    static QVariantList extractArchive(const QString &path);
    static QVariantList extractVideo(const QString &path);
#ifdef Q_OS_WIN
    static QVariantList extractExecutable(const QString &path);
    static QVariantList extractShortcut(const QString &path);
#endif
    static QVariantList extractDirectory(const QString &path);

    // Helper to append a {label, value} pair
    static inline void add(QVariantList &list, const QString &label, const QString &value) {
        if (!value.isEmpty())
            list.append(QVariantMap{{"label", label}, {"value", value}});
    }
};
