#pragma once

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>

class AppSettingsController;
class OperationQueue;
class ThemeController;
class QWindow;

class SystemTrayController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit SystemTrayController(QObject *parent = nullptr);

    void setSettings(AppSettingsController *settings);
    void setThemeController(ThemeController *theme);
    void setOperationQueue(OperationQueue *queue);
    void attachWindow(QWindow *window);

    bool available() const;
    bool active() const;

    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void hideWindow();

signals:
    void activeChanged();
    void optionsRequested();
    void exitRequested();

private:
    void applyTheme();
    void applyActionIcons();
    bool operationTaskbarActive() const;
    void syncTaskbarProgressWindow();
    void updateActiveState();
    void updateMenuState();

    AppSettingsController *m_settings = nullptr;
    OperationQueue *m_operationQueue = nullptr;
    ThemeController *m_theme = nullptr;
    QPointer<QWindow> m_window;
    QMenu m_menu;
    QSystemTrayIcon m_tray;
    QAction *m_showAction = nullptr;
    QAction *m_hideAction = nullptr;
    QAction *m_optionsAction = nullptr;
    QAction *m_exitAction = nullptr;
    bool m_active = false;
    bool m_taskbarProgressMinimized = false;
};
