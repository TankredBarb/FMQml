#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml>

class FmCheckBoxVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(int checkState READ checkState WRITE setCheckState NOTIFY checkStateChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool hovered READ hovered WRITE setHovered NOTIFY hoveredChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor markColor READ markColor WRITE setMarkColor NOTIFY colorsChanged)

public:
    explicit FmCheckBoxVisual(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal progress() const { return m_progress; }
    int checkState() const { return m_checkState; }
    bool active() const { return m_active; }
    bool hovered() const { return m_hovered; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor markColor() const { return m_markColor; }

    void setProgress(qreal value);
    void setCheckState(int value);
    void setActive(bool value);
    void setHovered(bool value);
    void setSurfaceColor(const QColor &value);
    void setBorderColor(const QColor &value);
    void setAccentColor(const QColor &value);
    void setMarkColor(const QColor &value);

signals:
    void progressChanged();
    void checkStateChanged();
    void activeChanged();
    void hoveredChanged();
    void colorsChanged();

private:
    qreal m_progress = 0.0;
    int m_checkState = 0;
    bool m_active = false;
    bool m_hovered = false;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_accentColor;
    QColor m_markColor;
};
