#pragma once

#include <QVariantList>
#include <QString>
#include <QtGlobal>

struct FileAttributesInfo {
    bool hidden = false;
    bool readOnly = false;
    bool system = false;
    bool archive = false;
};

struct FileAccessInfo {
    enum class State {
        Unknown,
        Denied,
        Allowed
    };

    bool canRead = false;
    bool canModify = false;
    bool canDelete = false;
    bool canExecute = false;
    bool canBrowse = false;
    bool canCreateChildren = false;
    bool canTraverse = false;
    bool canChangeAttributes = false;
    bool exact = false;
    State readState = State::Unknown;
    State modifyState = State::Unknown;
    State deleteState = State::Unknown;
    State executeState = State::Unknown;
    State browseState = State::Unknown;
    State createChildrenState = State::Unknown;
    State traverseState = State::Unknown;
    State changeAttributesState = State::Unknown;
};

struct FileUnixInfo {
    bool available = false;
    bool isSymbolicLink = false;
    QString owner;
    QString group;
    QString ownerName;
    QString groupName;
    quint64 ownerId = 0;
    quint64 groupId = 0;
    QString modeString;
    QString modeOctal;
    QString fileType;
    quint32 mode = 0;
    bool canChangeMode = false;
    bool setuid = false;
    bool setgid = false;
    bool sticky = false;
};

struct FileCapabilityInfo {
    QString path;
    bool exists = false;
    bool isDirectory = false;
    bool isArchiveLike = false;
    FileAttributesInfo attributes;
    FileAccessInfo access;
    FileUnixInfo unixInfo;
    QString accessSummary;
    QString attributesSummary;
};

class FileAccessResolver
{
public:
    static FileCapabilityInfo resolve(const QString &path);
    static QVariantList accessProperties(const FileCapabilityInfo &info);
    static QVariantList attributeProperties(const FileCapabilityInfo &info);
    static QVariantList unixProperties(const FileCapabilityInfo &info);
    static bool setHidden(const QString &path, bool enabled, QString *error = nullptr);
    static bool setReadOnly(const QString &path, bool enabled, QString *error = nullptr);
    static void invalidate(const QString &path);
    static void invalidateAll();
};
