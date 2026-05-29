#pragma once

#include "DirectoryChangeWatcher.h"

#include <QFileSystemWatcher>

class QtDirectoryChangeWatcher final : public DirectoryChangeWatcher {
    Q_OBJECT

public:
    explicit QtDirectoryChangeWatcher(QObject *parent = nullptr);
    ~QtDirectoryChangeWatcher() override = default;

    bool watch(const QString &path) override;
    void stop() override;
    QString watchedPath() const override;

private:
    QFileSystemWatcher m_watcher;
    QString m_watchedPath;
};
