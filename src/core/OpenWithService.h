#pragma once

#include "LaunchService.h"

#include <QList>
#include <QString>

#include <optional>

struct OpenWithTarget {
    QString path;
    QString displayName;
    QString mimeType;
    QString suffix;
    QString contentTypeKey;
    LaunchService::LaunchCategory category = LaunchService::LaunchCategory::Unsupported;
    bool isLocal = false;
    bool isLaunchable = false;
    QString blockedReason;
};

enum class OpenWithCandidateKind {
    Application,
    Wine,
    Proton,
    SystemChooser
};

struct OpenWithCandidate {
    QString id;
    QString displayName;
    QString iconName;
    OpenWithCandidateKind kind = OpenWithCandidateKind::Application;
    bool recommended = false;
    bool systemDefault = false;
    bool fmDefault = false;
    bool supportsMultipleFiles = false;
    bool available = true;
    QString unavailableReason;
};

struct OpenWithResult {
    bool ok = false;
    LaunchService::LaunchErrorCode errorCode = LaunchService::LaunchErrorCode::None;
    QString title;
    QString message;
    QString details;
    bool showDialog = false;
};

class OpenWithBackend {
public:
    virtual ~OpenWithBackend() = default;

    virtual QList<OpenWithCandidate> enumerateCandidates(const OpenWithTarget &target) const = 0;
    virtual OpenWithResult launch(const QList<OpenWithTarget> &targets, const OpenWithCandidate &candidate) const = 0;
};

class OpenWithService {
public:
    explicit OpenWithService(const OpenWithBackend *backend = nullptr);

    OpenWithTarget targetInfo(const QString &path) const;
    QList<OpenWithCandidate> candidatesForPath(const QString &path) const;
    OpenWithResult openWith(const QString &path, const QString &candidateId) const;
    OpenWithResult openWithMany(const QStringList &paths, const QString &candidateId) const;
    bool setPreferredCandidate(const QString &path, const QString &candidateId) const;
    void clearPreferredCandidate(const QString &path) const;
    std::optional<OpenWithCandidate> effectiveCandidate(const QString &path) const;

private:
    const OpenWithBackend *m_backend = nullptr;
};
