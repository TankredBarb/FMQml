#pragma once

#include <QObject>
#include <QLatin1String>
#include <QString>
#include <QStringList>

#include "../models/FavoritesModel.h"
#include "../core/FavoritesStore.h"

class IsoMountManager;

class FavoritesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(FavoritesModel *model READ model CONSTANT)
    Q_PROPERTY(FavoritesModel *pinnedModel READ pinnedModel CONSTANT)
    Q_PROPERTY(FavoritesModel *frequentModel READ frequentModel CONSTANT)
    Q_PROPERTY(int pinnedCount READ pinnedCount NOTIFY countsChanged)
    Q_PROPERTY(int frequentCount READ frequentCount NOTIFY countsChanged)
    Q_PROPERTY(int tagCount READ tagCount NOTIFY countsChanged)

public:
    static constexpr QLatin1String FAVORITES_ROOT{"favorites://"};

    explicit FavoritesController(QObject *parent = nullptr);

    FavoritesModel *model();
    FavoritesModel *pinnedModel();
    FavoritesModel *frequentModel();
    int pinnedCount() const;
    int frequentCount() const;
    int tagCount() const;

    Q_INVOKABLE bool pinPath(const QString &path);
    Q_INVOKABLE bool unpinPath(const QString &path);
    Q_INVOKABLE bool movePinnedUp(const QString &path);
    Q_INVOKABLE bool movePinnedDown(const QString &path);
    Q_INVOKABLE bool setPinnedLabel(const QString &path, const QString &label);
    Q_INVOKABLE bool setPinnedTags(const QString &path, const QStringList &tags);
    Q_INVOKABLE bool togglePinned(const QString &path);
    Q_INVOKABLE bool isPinned(const QString &path) const;
    Q_INVOKABLE int pinPaths(const QStringList &paths);
    Q_INVOKABLE int unpinPaths(const QStringList &paths);
    Q_INVOKABLE bool forgetUsagePath(const QString &path);
    Q_INVOKABLE bool clearFrequent();
    Q_INVOKABLE QStringList tagsForPath(const QString &path) const;
    Q_INVOKABLE void recordVisit(const QString &path);
    Q_INVOKABLE QString targetPathForItem(const QString &id) const;
    Q_INVOKABLE bool openItem(const QString &id);
    Q_INVOKABLE bool openPath(const QString &path);
    Q_INVOKABLE bool revealPath(const QString &path) const;
    Q_INVOKABLE bool openTerminalAtPath(const QString &path) const;

    void setIsoMountManager(IsoMountManager *manager);

signals:
    void countsChanged();
    void openPathRequested(const QString &path);

private:
    void refreshModel();

    FavoritesStore m_store;
    FavoritesModel m_model;
    FavoritesModel m_pinnedModel;
    FavoritesModel m_frequentModel;
    IsoMountManager *m_isoMountManager = nullptr;
};
