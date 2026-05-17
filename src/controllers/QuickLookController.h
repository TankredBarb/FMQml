#pragma once

#include <QObject>
#include <QString>
#include <atomic>

class QuickLookController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(QString content READ content NOTIFY contentChanged)
    Q_PROPERTY(QString type READ type NOTIFY typeChanged)
    Q_PROPERTY(QString extension READ extension NOTIFY extensionChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY sizeTextChanged)
    Q_PROPERTY(QString modifiedText READ modifiedText NOTIFY modifiedTextChanged)
    Q_PROPERTY(QString mimeName READ mimeName NOTIFY mimeNameChanged)
    Q_PROPERTY(bool directory READ directory NOTIFY directoryChanged)
    Q_PROPERTY(bool hidden READ hidden NOTIFY hiddenChanged)
    Q_PROPERTY(bool symlink READ symlink NOTIFY symlinkChanged)
    Q_PROPERTY(bool readable READ readable NOTIFY readableChanged)
    Q_PROPERTY(bool writable READ writable NOTIFY writableChanged)
    Q_PROPERTY(bool executable READ executable NOTIFY executableChanged)
    Q_PROPERTY(QString absolutePath READ absolutePath NOTIFY absolutePathChanged)
    Q_PROPERTY(QString parentPath READ parentPath NOTIFY parentPathChanged)
    Q_PROPERTY(QString canonicalPath READ canonicalPath NOTIFY canonicalPathChanged)
    Q_PROPERTY(QString permissionsText READ permissionsText NOTIFY permissionsTextChanged)
    Q_PROPERTY(int lines READ lines NOTIFY linesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)

public:
    explicit QuickLookController(QObject *parent = nullptr);

    QString path() const;
    QString content() const;
    QString type() const;
    QString extension() const;
    QString name() const;
    QString sizeText() const;
    QString modifiedText() const;
    QString mimeName() const;
    bool directory() const;
    bool hidden() const;
    bool symlink() const;
    bool readable() const;
    bool writable() const;
    bool executable() const;
    QString absolutePath() const;
    QString parentPath() const;
    QString canonicalPath() const;
    QString permissionsText() const;
    int lines() const;
    bool loading() const;
    bool visible() const;

    Q_INVOKABLE void preview(const QString &path);
    void setVisible(bool visible);

signals:
    void pathChanged();
    void contentChanged();
    void typeChanged();
    void extensionChanged();
    void nameChanged();
    void sizeTextChanged();
    void modifiedTextChanged();
    void mimeNameChanged();
    void directoryChanged();
    void hiddenChanged();
    void symlinkChanged();
    void readableChanged();
    void writableChanged();
    void executableChanged();
    void absolutePathChanged();
    void parentPathChanged();
    void canonicalPathChanged();
    void permissionsTextChanged();
    void linesChanged();
    void loadingChanged();
    void visibleChanged();

private:
    QString m_path;
    QString m_content;
    QString m_type;
    QString m_extension;
    QString m_name;
    QString m_sizeText;
    QString m_modifiedText;
    QString m_mimeName;
    bool m_directory = false;
    bool m_hidden = false;
    bool m_symlink = false;
    bool m_readable = false;
    bool m_writable = false;
    bool m_executable = false;
    QString m_absolutePath;
    QString m_parentPath;
    QString m_canonicalPath;
    QString m_permissionsText;
    int m_lines = 0;
    bool m_loading = false;
    bool m_visible = false;
    std::atomic<int> m_previewGeneration{0};
};
