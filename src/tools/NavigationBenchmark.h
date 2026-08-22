#pragma once

class QApplication;
class AppServices;
class QQuickWindow;

namespace NavigationBenchmark {
int run(QApplication &app);
int runSuite(QApplication &app);
int runGui(QApplication &app, AppServices &services, QQuickWindow &window);
}
