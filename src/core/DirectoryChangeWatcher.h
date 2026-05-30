#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <memory>

struct DirectoryChangeEvent {
    enum class Type {
        Added,
        Removed,
        Modified,
        Renamed,
        Overflow
    };

    Type type = Type::Overflow;
    QString path;
    QString oldPath;
    QString newPath;
    QString sourcePath;
};

Q_DECLARE_METATYPE(DirectoryChangeEvent)
Q_DECLARE_METATYPE(QList<DirectoryChangeEvent>)

class DirectoryChangeWatcher : public QObject {
    Q_OBJECT

public:
    explicit DirectoryChangeWatcher(QObject *parent = nullptr);
    ~DirectoryChangeWatcher() override = default;

    virtual bool watch(const QString &path) = 0;
    virtual void stop() = 0;
    virtual QString watchedPath() const = 0;

signals:
    void eventsReady(const QList<DirectoryChangeEvent> &events);
    void watchFailed(const QString &path, const QString &error);
};

std::unique_ptr<DirectoryChangeWatcher> createDirectoryChangeWatcher(QObject *parent = nullptr);
