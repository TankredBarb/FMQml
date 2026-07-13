#pragma once

#include <QObject>
#include <QThreadPool>
#include <QString>
#include <QVariantList>

class ProviderFolderSizeCalculator;

class ProviderPropertiesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool calculatingSize READ calculatingSize NOTIFY stateChanged)
    Q_PROPERTY(QString providerName READ providerName NOTIFY propertiesChanged)
    Q_PROPERTY(QString path READ path NOTIFY propertiesChanged)
    Q_PROPERTY(QString name READ name NOTIFY propertiesChanged)
    Q_PROPERTY(QString typeText READ typeText NOTIFY propertiesChanged)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY propertiesChanged)
    Q_PROPERTY(QString modifiedText READ modifiedText NOTIFY propertiesChanged)
    Q_PROPERTY(QString createdText READ createdText NOTIFY propertiesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QVariantList propertyGroups READ propertyGroups NOTIFY propertiesChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)

public:
    explicit ProviderPropertiesController(QObject *parent = nullptr);

    bool visible() const;
    bool loading() const;
    bool calculatingSize() const;
    QString providerName() const;
    QString path() const;
    QString name() const;
    QString typeText() const;
    QString sizeText() const;
    QString modifiedText() const;
    QString createdText() const;
    QString statusText() const;
    QVariantList propertyGroups() const;
    QString errorText() const;

    Q_INVOKABLE void load(const QString &path);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QString exportableText() const;
    Q_INVOKABLE QString exportableJson() const;

    void setVisible(bool visible);

signals:
    void visibleChanged();
    void propertiesChanged();
    void stateChanged();

private:
    void reset();
    void cancelCalculation();
    void rebuildPropertyGroups();
    void startFolderSizeCalculation();

private slots:
    void onSizeProgress(qint64 bytes, int files, int folders, bool exact, int generation);
    void onSizeCalculated(qint64 bytes,
                          int files,
                          int folders,
                          bool exact,
                          bool cancelled,
                          const QString &error,
                          int generation);

private:
    bool m_visible = false;
    bool m_loading = false;
    bool m_calculatingSize = false;
    bool m_sizeExact = false;
    int m_generation = 0;
    int m_fileCount = 0;
    int m_folderCount = 0;
    QString m_providerName;
    QString m_path;
    QString m_name;
    QString m_typeText;
    QString m_sizeText;
    QString m_itemCountText;
    QString m_modifiedText;
    QString m_createdText;
    QString m_statusText;
    QString m_errorText;
    QVariantList m_propertyGroups;
    QVariantList m_detailGroups;
    QVariantList m_quotaProperties;
    QThreadPool m_threadPool;
    ProviderFolderSizeCalculator *m_currentCalculator = nullptr;
};
