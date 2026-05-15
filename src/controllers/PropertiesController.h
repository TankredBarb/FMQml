#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
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

    Q_INVOKABLE void load(const QString &path);
    Q_INVOKABLE void cancelCalculation();
    void setVisible(bool visible);

signals:
    void propertiesChanged();
    void visibleChanged();
    void isCalculatingChanged();

private slots:
    void onSizeProgress(qint64 size, int generation);
    void onSizeCalculated(qint64 size, int generation);

private:
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
    int m_calcGeneration = 0;
    QThreadPool m_threadPool;
    FolderSizeCalculator *m_currentCalculator = nullptr;
};
