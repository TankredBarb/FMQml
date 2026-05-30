#pragma once

#include <QObject>
#include <QString>

class FileTypeIconResolver final : public QObject {
    Q_OBJECT

public:
    explicit FileTypeIconResolver(QObject *parent = nullptr);

    Q_INVOKABLE QString iconForSuffix(const QString &suffix, bool isDirectory) const;
    Q_INVOKABLE QString iconForPath(const QString &path) const;
    Q_INVOKABLE QString iconForPathHint(const QString &path, bool isDirectory) const;
};
