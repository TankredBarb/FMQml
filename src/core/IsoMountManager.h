#pragma once

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class IsoMountManager final : public QObject {
    Q_OBJECT

public:
    struct Mount {
        QString imagePath;
        QString rootPath;
        QChar letter;
        QChar requestedLetter;
        QDateTime mountedAt;
        quintptr nativeHandle = 0;
        QString nativeDevice;
        QString mountedDevice;
    };

    explicit IsoMountManager(QObject *parent = nullptr);

    Q_INVOKABLE bool canMountIsoPath(const QString &path) const;
    Q_INVOKABLE bool isMountedImage(const QString &imagePath) const;
    Q_INVOKABLE bool isManagedMountRoot(const QString &rootPath) const;
    QString managedMountRootForPath(const QString &path) const;
    Q_INVOKABLE bool isInsideManagedMount(const QString &path) const;
    Q_INVOKABLE QString mountedRootForImage(const QString &imagePath) const;
    Q_INVOKABLE void mountIsoToLetter(const QString &imagePath, const QString &letter);
    Q_INVOKABLE void unmountIsoRoot(const QString &rootPath);
    Q_INVOKABLE void unmountAll();

    QList<Mount> mounts() const;
    Mount mountForRoot(const QString &rootPath) const;

signals:
    void mountsChanged();
    void mountStarted(const QString &imagePath, const QString &rootPath);
    void mountFinished(const QString &imagePath, const QString &rootPath, bool success, const QString &error);
    void unmountStarted(const QString &rootPath);
    void unmountFinished(const QString &rootPath, bool success, const QString &error);
    void statusMessage(const QString &message);

private:
    static QString normalizedLocalPath(const QString &path);
    static QString normalizeRootPath(const QString &rootPath);
    static QChar normalizeLetter(const QString &letter);
    static QString rootPathForLetter(QChar letter);

    void adoptLinuxIsoMounts();
    void rememberMount(const QString &imagePath, const QString &rootPath, QChar requestedLetter,
                       quintptr nativeHandle, const QString &nativeDevice,
                       const QString &mountedDevice);
    void forgetMountRoot(const QString &rootPath);

    QHash<QString, Mount> m_mountsByRoot;
    QMultiHash<QString, QString> m_rootsByImage;
};
