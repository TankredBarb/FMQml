#include "ui/framework/FmDocumentViewVisual.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>

#include <cstdio>

namespace {
int fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 1;
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    FmDocumentViewVisual view;
    view.setWidth(640);
    view.setHeight(360);
    view.setFontFamily(QStringLiteral("Monospace"));
    view.setFontPixelSize(13);
    view.setText(QString(1024 * 1024, QLatin1Char('A')));

    if (view.documentHeight() <= 0 || view.documentHeight() > 100
        || view.documentWidth() <= view.width()) {
        return fail("unwrapped document geometry is inconsistent");
    }

    view.setContentX(view.documentWidth() - view.width());
    QImage image(640, 360, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    view.paint(&painter);
    painter.end();

    // Preview pages are bounded before they reach the visual. Keep wrapping
    // coverage representative of that contract instead of asking QTextLayout
    // to shape an artificial one-megabyte logical line.
    view.setText(QString(32 * 1024, QLatin1Char('A')));
    view.setWrap(true);
    if (view.documentWidth() != view.width() || view.documentHeight() <= view.height()) {
        return fail("wrapped document was not segmented into visual rows");
    }

    view.setText(QStringLiteral("one\ntwo\nthree"));
    if (view.documentHeight() <= 0 || view.documentHeight() >= 200) {
        return fail("small multiline document geometry is inconsistent");
    }

    view.setStyleRanges({QVariantMap{{QStringLiteral("start"), 0},
                                     {QStringLiteral("length"), 3},
                                     {QStringLiteral("role"), 1},
                                     {QStringLiteral("bold"), true},
                                     {QStringLiteral("italic"), true}}});
    view.setTokenColor1(QColor(QStringLiteral("#ff00ff")));
    if (view.styleRanges().size() != 1
        || !view.styleRanges().constFirst().toMap().value(QStringLiteral("bold")).toBool()
        || !view.styleRanges().constFirst().toMap().value(QStringLiteral("italic")).toBool()
        || view.tokenColor1() != QColor(QStringLiteral("#ff00ff"))) {
        return fail("styled document ranges were not retained");
    }

    return 0;
}
