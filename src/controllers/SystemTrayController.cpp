#include "SystemTrayController.h"

#include "AppSettingsController.h"
#include "../core/OperationQueue.h"
#include "ThemeController.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QSize>
#include <QSvgRenderer>
#include <QWindow>

namespace {
constexpr const char *AppIconPath = ":/qt/qml/FM/qml/assets/icons/app_icon.png";
constexpr const char *ShowIconPath = ":/qt/qml/FM/qml/assets/icons/eye.svg";
constexpr const char *HideIconPath = ":/qt/qml/FM/qml/assets/icons/eye-off.svg";
constexpr const char *OptionsIconPath = ":/qt/qml/FM/qml/assets/icons/settings.svg";
constexpr const char *ExitIconPath = ":/qt/qml/FM/qml/assets/icons/exit.svg";

QString cssColor(const QColor &color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

void recolorSvgAttribute(QString &svg, const QString &attribute, const QColor &color)
{
    const QRegularExpression pattern(
        QStringLiteral(R"((\b%1\s*=\s*["'])(?!none\b|transparent\b|url\()([^"']+)(["']))").arg(attribute),
        QRegularExpression::CaseInsensitiveOption);
    svg.replace(pattern, QStringLiteral("\\1%1\\3").arg(color.name(QColor::HexRgb)));
}

QImage renderSvgIcon(const QString &path, const QColor &color, const QSize &size)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QString svg = QString::fromUtf8(file.readAll());
    recolorSvgAttribute(svg, QStringLiteral("stroke"), color);
    recolorSvgAttribute(svg, QStringLiteral("fill"), color);

    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        return {};
    }

    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&painter);
    return image;
}

QIcon themedSvgIcon(const QString &path, const QColor &color)
{
    QIcon icon;
    for (const QSize size : {QSize(16, 16), QSize(20, 20), QSize(24, 24), QSize(32, 32)}) {
        const QImage image = renderSvgIcon(path, color, size);
        if (!image.isNull()) {
            icon.addPixmap(QPixmap::fromImage(image));
        }
    }
    return icon;
}
}

SystemTrayController::SystemTrayController(QObject *parent)
    : QObject(parent)
{
    m_tray.setIcon(QIcon(QString::fromLatin1(AppIconPath)));
    m_tray.setToolTip(QStringLiteral("FM"));

    m_showAction = m_menu.addAction(QStringLiteral("Show"));
    m_hideAction = m_menu.addAction(QStringLiteral("Hide"));
    m_optionsAction = m_menu.addAction(QStringLiteral("Options"));
    m_menu.addSeparator();
    m_exitAction = m_menu.addAction(QStringLiteral("Exit"));
    m_tray.setContextMenu(&m_menu);

    for (QAction *action : {m_showAction, m_hideAction, m_optionsAction, m_exitAction}) {
        action->setIconVisibleInMenu(true);
    }

    connect(m_showAction, &QAction::triggered, this, &SystemTrayController::showWindow);
    connect(m_hideAction, &QAction::triggered, this, &SystemTrayController::hideWindow);
    connect(m_optionsAction, &QAction::triggered, this, &SystemTrayController::optionsRequested);
    connect(m_exitAction, &QAction::triggered, this, &SystemTrayController::exitRequested);
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showWindow();
        }
    });

    applyTheme();
    updateMenuState();
}

void SystemTrayController::setSettings(AppSettingsController *settings)
{
    if (m_settings == settings) {
        return;
    }

    if (m_settings) {
        disconnect(m_settings, nullptr, this, nullptr);
    }

    m_settings = settings;
    if (m_settings) {
        connect(m_settings, &AppSettingsController::useSystemTrayIconChanged,
                this, &SystemTrayController::updateActiveState);
    }
    updateActiveState();
}

void SystemTrayController::setThemeController(ThemeController *theme)
{
    if (m_theme == theme) {
        return;
    }

    if (m_theme) {
        disconnect(m_theme, nullptr, this, nullptr);
    }

    m_theme = theme;
    if (m_theme) {
        connect(m_theme, &ThemeController::themeChanged, this, &SystemTrayController::applyTheme);
    }
    applyTheme();
}

void SystemTrayController::setOperationQueue(OperationQueue *queue)
{
    if (m_operationQueue == queue) {
        return;
    }

    if (m_operationQueue) {
        disconnect(m_operationQueue, nullptr, this, nullptr);
    }

    m_operationQueue = queue;
    if (m_operationQueue) {
        connect(m_operationQueue, &OperationQueue::busyChanged,
                this, &SystemTrayController::syncTaskbarProgressWindow);
        connect(m_operationQueue, &OperationQueue::errorChanged,
                this, &SystemTrayController::syncTaskbarProgressWindow);
    }
    syncTaskbarProgressWindow();
}

void SystemTrayController::attachWindow(QWindow *window)
{
    if (m_window == window) {
        return;
    }

    if (m_window) {
        disconnect(m_window.data(), nullptr, this, nullptr);
    }

    m_window = window;
    if (m_window) {
        connect(m_window.data(), &QWindow::visibleChanged, this, [this]() {
            updateMenuState();
            syncTaskbarProgressWindow();
        });
        connect(m_window.data(), &QWindow::visibilityChanged, this, [this]() {
            updateMenuState();
            syncTaskbarProgressWindow();
        });
        connect(m_window.data(), &QWindow::windowStateChanged, this, [this]() {
            updateMenuState();
            syncTaskbarProgressWindow();
        });
    }
    updateMenuState();
}

void SystemTrayController::applyTheme()
{
    const QColor surface = m_theme
        ? (m_theme->isDark() ? m_theme->surface() : m_theme->bg())
        : QApplication::palette().color(QPalette::Window);
    const QColor text = m_theme ? m_theme->textPrimary() : QApplication::palette().color(QPalette::WindowText);
    const QColor secondaryText = m_theme ? m_theme->textSecondary() : QApplication::palette().color(QPalette::Disabled, QPalette::WindowText);
    const QColor border = m_theme
        ? withAlpha(m_theme->menuBorder(), m_theme->isDark() ? 0.40 : 0.28)
        : QApplication::palette().color(QPalette::Mid);
    const QColor hover = m_theme ? m_theme->surfaceHover() : QApplication::palette().color(QPalette::Highlight);
    const QColor pressed = m_theme ? m_theme->menuItemPressed() : hover;
    const QColor separator = m_theme ? m_theme->menuSeparator() : QApplication::palette().color(QPalette::Mid);

    QPalette palette = m_menu.palette();
    palette.setColor(QPalette::Window, surface);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, secondaryText);
    palette.setColor(QPalette::Highlight, hover);
    palette.setColor(QPalette::HighlightedText, text);
    m_menu.setPalette(palette);

    m_menu.setStyleSheet(QStringLiteral(
        "QMenu {"
        " background-color: %1;"
        " color: %2;"
        " border: 1px solid %3;"
        " padding: 6px 4px;"
        "}"
        "QMenu::item {"
        " min-width: 132px;"
        " padding: 7px 24px 7px 30px;"
        " margin: 1px 4px;"
        " border-radius: 5px;"
        "}"
        "QMenu::item:selected {"
        " background-color: %4;"
        " color: %2;"
        "}"
        "QMenu::item:pressed {"
        " background-color: %5;"
        "}"
        "QMenu::item:disabled {"
        " color: %6;"
        "}"
        "QMenu::icon {"
        " width: 16px;"
        " height: 16px;"
        "}"
        "QMenu::separator {"
        " height: 1px;"
        " background: %7;"
        " margin: 5px 8px;"
        "}"
    ).arg(cssColor(surface),
          cssColor(text),
          cssColor(border),
          cssColor(hover),
          cssColor(pressed),
          cssColor(secondaryText),
          cssColor(separator)));

    applyActionIcons();
}

void SystemTrayController::applyActionIcons()
{
    const QColor navigation = m_theme ? m_theme->categoryNavigation() : QApplication::palette().color(QPalette::Highlight);
    const QColor utility = m_theme ? m_theme->categoryUtility() : QApplication::palette().color(QPalette::WindowText);
    const QColor secondary = m_theme ? m_theme->textSecondary() : QApplication::palette().color(QPalette::WindowText);
    const QColor danger = m_theme ? m_theme->danger() : QColor(QStringLiteral("#DC2626"));

    if (m_showAction) {
        m_showAction->setIcon(themedSvgIcon(QString::fromLatin1(ShowIconPath), navigation));
    }
    if (m_hideAction) {
        m_hideAction->setIcon(themedSvgIcon(QString::fromLatin1(HideIconPath), secondary));
    }
    if (m_optionsAction) {
        m_optionsAction->setIcon(themedSvgIcon(QString::fromLatin1(OptionsIconPath), utility));
    }
    if (m_exitAction) {
        m_exitAction->setIcon(themedSvgIcon(QString::fromLatin1(ExitIconPath), danger));
    }
}

bool SystemTrayController::available() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

bool SystemTrayController::active() const
{
    return m_active;
}

bool SystemTrayController::operationTaskbarActive() const
{
    return m_operationQueue && (m_operationQueue->busy() || !m_operationQueue->error().isEmpty());
}

void SystemTrayController::syncTaskbarProgressWindow()
{
    if (!m_window) {
        m_taskbarProgressMinimized = false;
        updateMenuState();
        return;
    }

    const bool minimized = m_window->windowStates().testFlag(Qt::WindowMinimized);
    const bool restored = m_window->isVisible() && !minimized;
    if (m_taskbarProgressMinimized && restored) {
        m_taskbarProgressMinimized = false;
    }

    if (!m_active || !operationTaskbarActive()) {
        if (m_taskbarProgressMinimized && m_window->isVisible() && minimized) {
            m_taskbarProgressMinimized = false;
            m_window->hide();
        } else if (!operationTaskbarActive()) {
            m_taskbarProgressMinimized = false;
        }
        updateMenuState();
        return;
    }

    if (!m_window->isVisible()) {
        m_taskbarProgressMinimized = true;
        m_window->showMinimized();
    }
    updateMenuState();
}

void SystemTrayController::showWindow()
{
    if (!m_window) {
        return;
    }

    const Qt::WindowStates states = m_window->windowStates() & ~Qt::WindowMinimized;
    m_taskbarProgressMinimized = false;
    m_window->setWindowStates(states);
    if (states.testFlag(Qt::WindowFullScreen)) {
        m_window->showFullScreen();
    } else if (states.testFlag(Qt::WindowMaximized)) {
        m_window->showMaximized();
    } else {
        m_window->show();
    }
    m_window->raise();
    m_window->requestActivate();
    updateMenuState();
}

void SystemTrayController::hideWindow()
{
    if (!m_active || !m_window) {
        return;
    }

    if (operationTaskbarActive()) {
        m_taskbarProgressMinimized = true;
        m_window->showMinimized();
    } else {
        m_taskbarProgressMinimized = false;
        m_window->hide();
    }
    updateMenuState();
}

void SystemTrayController::updateActiveState()
{
    const bool nextActive = available() && m_settings && m_settings->useSystemTrayIcon();
    if (m_active == nextActive) {
        updateMenuState();
        return;
    }

    m_active = nextActive;
    QApplication::setQuitOnLastWindowClosed(!m_active);
    if (m_active) {
        m_tray.show();
    } else {
        m_tray.hide();
        m_taskbarProgressMinimized = false;
    }
    syncTaskbarProgressWindow();
    updateMenuState();
    emit activeChanged();
}

void SystemTrayController::updateMenuState()
{
    const bool minimized = m_window && m_window->windowStates().testFlag(Qt::WindowMinimized);
    const bool visible = m_window && m_window->isVisible() && !minimized;
    if (m_taskbarProgressMinimized && visible) {
        m_taskbarProgressMinimized = false;
    }
    if (m_showAction) {
        m_showAction->setEnabled(m_active && m_window && (!m_window->isVisible() || minimized));
    }
    if (m_hideAction) {
        m_hideAction->setEnabled(m_active && visible);
    }
    if (m_optionsAction) {
        m_optionsAction->setEnabled(m_active);
    }
    if (m_exitAction) {
        m_exitAction->setEnabled(m_active);
    }
}
