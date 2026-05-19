#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QThreadPool>

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
    Q_PROPERTY(bool isCalculating READ isCalculating NOTIFY isCalculatingChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QVariantList extraProperties READ extraProperties NOTIFY propertiesChanged)
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
    bool isCalculating() const;
    bool visible() const;
    QVariantList extraProperties() const;
    int fileCount() const;
    int folderCount() const;
    int selectedCount() const;
    QStringList selectedPaths() const;

    Q_INVOKABLE void load(const QString &path);
    Q_INVOKABLE void loadMultiple(const QStringList &paths);
    Q_INVOKABLE void cancelCalculation();
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

    QString m_name;
    QString m_path;
    QString m_sizeText;
    QString m_typeText;
    QString m_created;
    QString m_modified;
    QString m_accessed;
    bool m_isDirectory = false;
    bool m_isCalculating = false;
    bool m_visible = false;
    int m_fileCount = 0;
    int m_folderCount = 0;
    int m_calcGeneration = 0;
    QVariantList m_extraProperties;
    QThreadPool m_threadPool;
    FolderSizeCalculator *m_currentCalculator = nullptr;

    // Multi-selection state
    int m_selectedCount = 0;
    QStringList m_selectedPaths;

    // Multi-selection calculation
    qint64 m_multiTotalSize = 0;
    int m_multiFileCount = 0;
    int m_multiFolderCount = 0;
    int m_multiPendingCalcs = 0;   // how many folder calculators are still running
    QList<FolderSizeCalculator *> m_multiCalculators;
};
