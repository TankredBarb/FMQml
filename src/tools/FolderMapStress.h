#pragma once

class AppServices;
class QQuickWindow;
class QString;

namespace FolderMapStress {
int run(AppServices &services, QQuickWindow &window, const QString &fixturePath);
}
