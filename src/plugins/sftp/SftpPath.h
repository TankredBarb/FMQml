#pragma once

#include <QString>
#include <QUrl>

namespace SftpPath {
struct Address {
    QUrl url;
    QString error;
    bool valid() const { return error.isEmpty(); }
    QString encoded() const { return url.toString(QUrl::FullyEncoded); }
    QString identity() const;
};

bool isSchemePath(const QString &path);
Address parse(const QString &path);
Address fromFields(const QString &host, int port, const QString &user, const QString &path);
QString parent(const QString &path);
QString child(const QString &path, const QString &name);
QString fileName(const QString &path);
}
