#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

class FilePanelController;
class FolderPeekController;
class QuickLookController;

class QuickLookNavigationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY stateChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY stateChanged)
    Q_PROPERTY(int originKind READ originKind NOTIFY stateChanged)

public:
    enum OriginKind { NoOrigin = 0, PanelOrigin, FolderPeekOrigin };
    Q_ENUM(OriginKind)

    explicit QuickLookNavigationController(QObject *parent = nullptr);

    void setQuickLookController(QuickLookController *controller);
    bool active() const { return m_originKind != NoOrigin; }
    bool canGoPrevious() const { return m_canGoPrevious; }
    bool canGoNext() const { return m_canGoNext; }
    int originKind() const { return m_originKind; }

    Q_INVOKABLE bool beginPanel(QObject *panel, const QString &path);
    Q_INVOKABLE bool beginFolderPeek(QObject *peek, const QString &path);
    Q_INVOKABLE void endSession();
    Q_INVOKABLE bool navigate(int direction);

signals:
    void stateChanged();
    void targetCommitted(const QString &path, int originKind, quint64 revision);

private:
    int count() const;
    QString pathAt(int row) const;
    bool ordinaryAt(int row) const;
    int indexOfPath(const QString &path) const;
    int adjacentRow(int fromRow, int direction) const;
    bool originStillValid() const;
    void refresh();
    bool commitRow(int row);
    void disconnectOrigin();

    QPointer<QuickLookController> m_quickLook;
    QPointer<FilePanelController> m_panel;
    QPointer<FolderPeekController> m_peek;
    QVector<QMetaObject::Connection> m_connections;
    QString m_directoryIdentity;
    QString m_requestedPath;
    int m_requestedRow = -1;
    OriginKind m_originKind = NoOrigin;
    quint64 m_revision = 0;
    bool m_canGoPrevious = false;
    bool m_canGoNext = false;
};
