#pragma once

#include <QString>

enum class PathKind {
    Empty,
    Local,
    FileUrl,
    Archive,
    DevicesRoot,
    FavoritesRoot,
    Provider,
    Unknown
};

struct PathDescriptor {
    PathKind kind = PathKind::Empty;
    QString scheme;
};

class PathSemantics final {
public:
    static PathDescriptor describe(const QString &path);
    static QString explicitScheme(const QString &path);
    static bool hasExplicitNonLocalScheme(const QString &path);
    static bool isProviderPath(const QString &path);
    static QString localPathFromFileUrl(const QString &path);
    static QString fileUrlFromLocalPath(const QString &path);
    static QString compatibilityKindName(const PathDescriptor &descriptor);
};
