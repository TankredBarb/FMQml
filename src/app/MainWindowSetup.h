#pragma once

class QApplication;
class QQuickWindow;
class ThemeController;

namespace MainWindowSetup {
void configureProcessIdentity();
void configureApplication(QApplication &app);
void configureMainWindow(QQuickWindow *window, ThemeController *theme);
}
