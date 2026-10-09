#pragma once

#include <QElapsedTimer>
#include <QAbstractItemModel>
#include <QPointer>
#include <QTimer>
#include <QHash>
#include <QImage>
#include <QQuickItem>
#include <QSet>
#include <QtQml/qqmlregistration.h>
#include <atomic>
#include <memory>

class DirectoryChangeWatcher;

class FolderMapItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QAbstractItemModel *directoryModel READ directoryModel WRITE setDirectoryModel NOTIFY directoryModelChanged)
    Q_PROPERTY(QString currentPath READ currentPath WRITE setCurrentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(bool gpuEnabled READ gpuEnabled WRITE setGpuEnabled NOTIFY gpuEnabledChanged)
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths WRITE setSelectedPaths NOTIFY selectedPathsChanged)
    Q_PROPERTY(QColor folderColor MEMBER m_folderColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor fileColor MEMBER m_fileColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor textColor MEMBER m_textColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentColor MEMBER m_accentColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor borderColor MEMBER m_borderColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor headerColor MEMBER m_headerColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor gradientStartColor MEMBER m_gradientStartColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor gradientEndColor MEMBER m_gradientEndColor NOTIFY paletteChanged)
    Q_PROPERTY(qreal gradientStrength MEMBER m_gradientStrength NOTIFY paletteChanged)
    Q_PROPERTY(bool showThumbnails READ showThumbnails WRITE setShowThumbnails NOTIFY showThumbnailsChanged)
    Q_PROPERTY(int thumbnailsReady READ thumbnailsReady NOTIFY thumbnailsChanged)
    Q_PROPERTY(bool useNativeIcons READ useNativeIcons WRITE setUseNativeIcons NOTIFY useNativeIconsChanged)
    Q_PROPERTY(bool iconsEnabled READ iconsEnabled WRITE setIconsEnabled NOTIFY iconsChanged)
    Q_PROPERTY(bool iconsReady READ iconsReady NOTIFY iconsChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool moving READ moving NOTIFY movingChanged)
    Q_PROPERTY(int loadedCount READ loadedCount NOTIFY treeChanged)
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY treeChanged)
public:
    explicit FolderMapItem(QQuickItem *parent = nullptr);
    ~FolderMapItem() override;
    static void waitForIconAtlases();
    QAbstractItemModel *directoryModel() const { return m_directoryModel; }
    void setDirectoryModel(QAbstractItemModel *model);
    QString currentPath() const { return m_currentPath; }
    void setCurrentPath(const QString &path);
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool value);
    bool gpuEnabled() const { return m_gpuEnabled; }
    void setGpuEnabled(bool value);
    QStringList selectedPaths() const { return m_selectedPaths; }
    void setSelectedPaths(const QStringList &paths);
    bool iconsEnabled() const { return m_iconsEnabled; }
    void setIconsEnabled(bool enabled);
    bool showThumbnails() const { return m_showThumbnails; }
    void setShowThumbnails(bool enabled);
    int thumbnailsReady() const;
    bool useNativeIcons() const { return m_useNativeIcons; }
    void setUseNativeIcons(bool enabled);
    bool iconsReady() const { return !m_iconAtlases[m_useNativeIcons].isNull(); }
    QString status() const { return m_status; }
    bool moving() const { return m_moving; }
    int loadedCount() const { return int(m_byPath.size()); }
    int visibleCount() const { return int(m_visible.size()); }
    Q_INVOKABLE QVariantMap entryAt(qreal x, qreal y) const;
    Q_INVOKABLE QVariantMap entryForPath(const QString &path) const;
    Q_INVOKABLE QVariantMap selectionContext(const QString &path) const;
    Q_INVOKABLE QRectF screenRect(const QString &path) const;
    Q_INVOKABLE void zoomAt(qreal x, qreal y, qreal steps);
    Q_INVOKABLE void panBy(qreal dx, qreal dy);
    Q_INVOKABLE void fitCurrent();
    Q_INVOKABLE void revealPath(const QString &path);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void remapEntry(const QString &oldPath, const QString &newPath);
signals:
    void directoryModelChanged();
    void currentPathChanged();
    void showHiddenChanged();
    void gpuEnabledChanged();
    void selectedPathsChanged();
    void paletteChanged();
    void iconsChanged();
    void useNativeIconsChanged();
    void showThumbnailsChanged();
    void thumbnailsChanged();
    void statusChanged();
    void movingChanged();
    void treeChanged();
    void entriesChanged();
protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void updatePolish() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
private slots:
    void thumbnailFinished();
    void queueModelSync();
private:
    friend class FolderMapTest;
    friend class FolderMapStressProbe;
    struct Thumbnail;
    void scheduleThumbnails();
    void clearThumbnails();
    struct Entry { QString path; QString name; bool directory = false; bool symlink = false; int icon = 6; };
    struct Listing { QList<Entry> entries; QString message; };
    struct Node {
        Entry entry;
        int parent = -1;
        QRectF rect;
        QList<int> children;
        QString message;
        int columns = 1;
        bool requested = false;
        bool loaded = false;
        quint64 listingRevision = 0;
    };
    struct Visible { int index; QRectF rect; QString label; QString path; bool selected; };
    void resetTree();
    void syncModel();
    void syncWatchers();
    void reloadChangedFolders();
    void applyListing(int index, const Listing &listing);
    void removeBranch(int index);
    void layoutChildren(int index);
    void invalidateThumbnail(const QString &path);
    void requestListing(int index);
    void collectVisible(int index, const QRectF &viewport, QList<int> &requests);
    void startMotion();
    qreal minimumScale() const;
    void constrainCamera(QPointF &center, qreal &scale) const;
    void ensureIconAtlas();
    void setStatus(const QString &text);
    static Listing readDirectory(const QString &path, bool hidden, const std::shared_ptr<std::atomic_bool> &cancelled);
    QPointer<QAbstractItemModel> m_directoryModel;
    QList<QMetaObject::Connection> m_modelConnections;
    QTimer m_modelTimer;
    QTimer m_watchTimer;
    QHash<QString, std::shared_ptr<DirectoryChangeWatcher>> m_watchers;
    QSet<QString> m_watchFailures;
    QSet<QString> m_dirtyFolders;
    QList<int> m_freeNodes;
    quint64 m_listingSequence = 0;
    QList<Node> m_nodes;
    QHash<QString, int> m_byPath;
    QList<Visible> m_visible;
    std::shared_ptr<std::atomic_bool> m_cancelled = std::make_shared<std::atomic_bool>(false);
    QString m_currentPath;
    QString m_status;
    QStringList m_selectedPaths;
    QSet<QString> m_selection;
    bool m_showHidden = false;
    bool m_gpuEnabled = false;
    bool m_moving = false;
    bool m_cameraInitialized = false;
    int m_loading = 0;
    quint64 m_generation = 0;
    QPointF m_center{512, 384};
    QPointF m_targetCenter{512, 384};
    qreal m_scale = 1;
    qreal m_targetScale = 1;
    QElapsedTimer m_clock;
    QHash<QString, std::shared_ptr<Thumbnail>> m_thumbnails;
    bool m_showThumbnails = true;
    QImage m_iconAtlas;
    bool m_iconsEnabled = true;
    bool m_useNativeIcons = true;
    QImage m_iconAtlases[2];
    bool m_atlasRequested[2]{};
    QMetaObject::Connection m_afterAnimating;
    QColor m_folderColor{"#26374b"};
    QColor m_fileColor{"#202a36"};
    QColor m_textColor{"#e3eaf2"};
    QColor m_accentColor{"#72b7f0"};
    QColor m_borderColor{"#4b6580"};
    QColor m_headerColor{"#304459"};
    QColor m_gradientStartColor;
    QColor m_gradientEndColor;
    qreal m_gradientStrength = 0;
};
