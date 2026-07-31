#pragma once

#include <QObject>
#include <QList>
#include <QReadWriteLock>
#include <QString>
#include <QVariantList>

class FileTypeIconResolver final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList iconOverrides READ iconOverrides NOTIFY iconOverridesChanged)
    Q_PROPERTY(int iconOverrideRevision READ iconOverrideRevision NOTIFY iconOverridesChanged)

public:
    explicit FileTypeIconResolver(QObject *parent = nullptr);

    Q_INVOKABLE QString iconForSuffix(const QString &suffix, bool isDirectory) const;
    Q_INVOKABLE QString iconForPath(const QString &path) const;
    Q_INVOKABLE QString iconForPathHint(const QString &path, bool isDirectory) const;
    Q_INVOKABLE QString nativeIconOverrideForPathHint(const QString &path, bool isDirectory) const;
    QVariantList iconOverrides() const;
    int iconOverrideRevision() const;
    Q_INVOKABLE bool addOrUpdateIconOverride(const QString &suffix, const QString &sourceType, const QString &sourceValue);
    Q_INVOKABLE bool removeIconOverride(const QString &suffix);
    Q_INVOKABLE void clearIconOverrides();
    Q_INVOKABLE QStringList availableBundledIconNames() const;
    void reloadIconOverrides();

signals:
    void iconOverridesChanged();

private:
    struct IconOverride { QString suffix; QString sourceType; QString sourceValue; };
    QString normalizedSuffix(QString suffix) const;
    const IconOverride *matchingOverride(const QString &path) const;
    void loadIconOverrides();
    void saveIconOverrides() const;
    mutable QReadWriteLock m_iconOverridesLock;
    QList<IconOverride> m_iconOverrides;
    int m_iconOverrideRevision = 0;
};
