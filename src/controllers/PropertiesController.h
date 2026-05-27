#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <QStringList>
#include <QThreadPool>
#include <QVariantList>
#include "../core/FileAccessResolver.h"

class FolderSizeCalculator;

class PropertiesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY propertiesChanged)
    Q_PROPERTY(QString path READ path NOTIFY propertiesChanged)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY propertiesChanged)
    Q_PROPERTY(QString typeText READ typeText NOTIFY propertiesChanged)
    Q_PROPERTY(QString created READ created NOTIFY propertiesChanged)
    Q_PROPERTY(QString modified READ modified NOTIFY propertiesChanged)
    Q_PROPERTY(QString accessed READ accessed NOTIFY propertiesChanged)
    Q_PROPERTY(bool isDirectory READ isDirectory NOTIFY propertiesChanged)
    Q_PROPERTY(bool isDrive READ isDrive NOTIFY propertiesChanged)
    Q_PROPERTY(QString driveRootPath READ driveRootPath NOTIFY propertiesChanged)
    Q_PROPERTY(QString driveFileSystem READ driveFileSystem NOTIFY propertiesChanged)
    Q_PROPERTY(QString driveType READ driveType NOTIFY propertiesChanged)
    Q_PROPERTY(QString driveUsedText READ driveUsedText NOTIFY propertiesChanged)
    Q_PROPERTY(QString driveFreeText READ driveFreeText NOTIFY propertiesChanged)
    Q_PROPERTY(QString driveTotalText READ driveTotalText NOTIFY propertiesChanged)
    Q_PROPERTY(double driveUsagePercent READ driveUsagePercent NOTIFY propertiesChanged)
    Q_PROPERTY(bool driveReady READ driveReady NOTIFY propertiesChanged)
    Q_PROPERTY(bool driveCritical READ driveCritical NOTIFY propertiesChanged)
    Q_PROPERTY(bool isCalculating READ isCalculating NOTIFY isCalculatingChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QVariantList extraProperties READ extraProperties NOTIFY propertiesChanged)
    Q_PROPERTY(QVariantList accessProperties READ accessProperties NOTIFY propertiesChanged)
    Q_PROPERTY(QVariantList attributeProperties READ attributeProperties NOTIFY propertiesChanged)
    Q_PROPERTY(bool canEditAttributes READ canEditAttributes NOTIFY propertiesChanged)
    Q_PROPERTY(bool hiddenAttribute READ hiddenAttribute NOTIFY propertiesChanged)
    Q_PROPERTY(bool readOnlyAttribute READ readOnlyAttribute NOTIFY propertiesChanged)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY propertiesChanged)
    Q_PROPERTY(int folderCount READ folderCount NOTIFY propertiesChanged)
    // Multi-selection
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY propertiesChanged)
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths NOTIFY propertiesChanged)

public:
    explicit PropertiesController(QObject *parent = nullptr);

    QString name() const;
    QString path() const;
    QString sizeText() const;
    QString typeText() const;
    QString created() const;
    QString modified() const;
    QString accessed() const;
    bool isDirectory() const;
    bool isDrive() const;
    QString driveRootPath() const;
    QString driveFileSystem() const;
    QString driveType() const;
    QString driveUsedText() const;
    QString driveFreeText() const;
    QString driveTotalText() const;
    double driveUsagePercent() const;
    bool driveReady() const;
    bool driveCritical() const;
    bool isCalculating() const;
    bool visible() const;
    QVariantList extraProperties() const;
    QVariantList accessProperties() const;
    QVariantList attributeProperties() const;
    bool canEditAttributes() const;
    bool hiddenAttribute() const;
    bool readOnlyAttribute() const;
    int fileCount() const;
    int folderCount() const;
    int selectedCount() const;
    QStringList selectedPaths() const;

    Q_INVOKABLE void load(const QString &path);
    Q_INVOKABLE void loadMultiple(const QStringList &paths);
    Q_INVOKABLE void cancelCalculation();
    Q_INVOKABLE bool setHiddenAttribute(bool enabled);
    Q_INVOKABLE bool setReadOnlyAttribute(bool enabled);
    void setVisible(bool visible);

signals:
    void propertiesChanged();
    void visibleChanged();
    void isCalculatingChanged();

private slots:
    void onSizeProgress(qint64 size, int files, int folders, int generation);
    void onSizeCalculated(qint64 size, int files, int folders, int generation);
    // For multi-selection parallel calculators
    void onMultiSizeProgress(qint64 size, int files, int folders, int generation);
    void onMultiSizeCalculated(qint64 size, int files, int folders, int generation);

private:
    void cancelAllCalculators();
    void emitProgressUpdate();
    void resetDriveProperties();
    bool tryLoadDrive(const QString &path);
    void updateAttributeState(const FileCapabilityInfo &capabilities);

    QString m_name;
    QString m_path;
    QString m_sizeText;
    QString m_typeText;
    QString m_created;
    QString m_modified;
    QString m_accessed;
    bool m_isDirectory = false;
    bool m_isDrive = false;
    QString m_driveRootPath;
    QString m_driveFileSystem;
    QString m_driveType;
    QString m_driveUsedText;
    QString m_driveFreeText;
    QString m_driveTotalText;
    double m_driveUsagePercent = 0.0;
    bool m_driveReady = false;
    bool m_driveCritical = false;
    bool m_isCalculating = false;
    bool m_visible = false;
    int m_fileCount = 0;
    int m_folderCount = 0;
    int m_calcGeneration = 0;
    QVariantList m_extraProperties;
    QVariantList m_accessProperties;
    QVariantList m_attributeProperties;
    bool m_canEditAttributes = false;
    bool m_hiddenAttribute = false;
    bool m_readOnlyAttribute = false;
    QThreadPool m_threadPool;
    FolderSizeCalculator *m_currentCalculator = nullptr;

    // Multi-selection state
    int m_selectedCount = 0;
    QStringList m_selectedPaths;

    // Multi-selection calculation
    qint64 m_multiTotalSize = 0;
    qint64 m_multiBaseSize = 0;
    int m_multiFileCount = 0;
    int m_multiFolderCount = 0;
    int m_multiBaseFileCount = 0;
    int m_multiBaseFolderCount = 0;
    int m_multiPendingCalcs = 0;   // how many folder calculators are still running
    QList<FolderSizeCalculator *> m_multiCalculators;
    QHash<FolderSizeCalculator *, qint64> m_multiFolderSizes;
    QHash<FolderSizeCalculator *, int> m_multiFolderFileCounts;
    QHash<FolderSizeCalculator *, int> m_multiFolderFolderCounts;
};
