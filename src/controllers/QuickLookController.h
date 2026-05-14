#pragma once

#include <QObject>
#include <QString>

class QuickLookController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(QString content READ content NOTIFY contentChanged)
    Q_PROPERTY(QString type READ type NOTIFY typeChanged)
    Q_PROPERTY(QString extension READ extension NOTIFY extensionChanged)
    Q_PROPERTY(int lines READ lines NOTIFY linesChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)

public:
    explicit QuickLookController(QObject *parent = nullptr);

    QString path() const;
    QString content() const;
    QString type() const;
    QString extension() const;
    int lines() const;
    bool visible() const;

    Q_INVOKABLE void preview(const QString &path);
    void setVisible(bool visible);

signals:
    void pathChanged();
    void contentChanged();
    void typeChanged();
    void extensionChanged();
    void linesChanged();
    void visibleChanged();

private:
    QString m_path;
    QString m_content;
    QString m_type;
    QString m_extension;
    int m_lines = 0;
    bool m_visible = false;
};
