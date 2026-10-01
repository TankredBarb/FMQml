#pragma once

class AppServices;
class QQuickWindow;
class QString;

namespace PanelResizeBenchmark {
int run(AppServices &services, QQuickWindow &window, const QString &photosPath);
}
