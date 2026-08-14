#pragma once

#include <QColor>
#include <QFont>
#include <QQuickPaintedItem>
#include <QtQml>
#include <QVariantList>

class FmDocumentViewVisual : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY documentChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY layoutChanged)
    Q_PROPERTY(qreal fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY layoutChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY visualChanged)
    Q_PROPERTY(QColor lineNumberColor READ lineNumberColor WRITE setLineNumberColor NOTIFY visualChanged)
    Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor NOTIFY visualChanged)
    Q_PROPERTY(QColor selectedTextColor READ selectedTextColor WRITE setSelectedTextColor NOTIFY visualChanged)
    Q_PROPERTY(QColor gutterColor READ gutterColor WRITE setGutterColor NOTIFY visualChanged)
    Q_PROPERTY(QColor dividerColor READ dividerColor WRITE setDividerColor NOTIFY visualChanged)
    Q_PROPERTY(QVariantList styleRanges READ styleRanges WRITE setStyleRanges NOTIFY visualChanged)
    Q_PROPERTY(QColor tokenColor1 READ tokenColor1 WRITE setTokenColor1 NOTIFY visualChanged)
    Q_PROPERTY(QColor tokenColor2 READ tokenColor2 WRITE setTokenColor2 NOTIFY visualChanged)
    Q_PROPERTY(QColor tokenColor3 READ tokenColor3 WRITE setTokenColor3 NOTIFY visualChanged)
    Q_PROPERTY(QColor tokenColor4 READ tokenColor4 WRITE setTokenColor4 NOTIFY visualChanged)
    Q_PROPERTY(bool wrap READ wrap WRITE setWrap NOTIFY layoutChanged)
    Q_PROPERTY(bool showLineNumbers READ showLineNumbers WRITE setShowLineNumbers NOTIFY layoutChanged)
    Q_PROPERTY(qint64 firstLine READ firstLine WRITE setFirstLine NOTIFY visualChanged)
    Q_PROPERTY(qreal contentX READ contentX WRITE setContentX NOTIFY viewportChanged)
    Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY viewportChanged)
    Q_PROPERTY(qreal documentWidth READ documentWidth NOTIFY documentGeometryChanged)
    Q_PROPERTY(qreal documentHeight READ documentHeight NOTIFY documentGeometryChanged)
    Q_PROPERTY(qreal textPadding READ textPadding WRITE setTextPadding NOTIFY layoutChanged)
    Q_PROPERTY(qreal lineNumberWidth READ lineNumberWidth WRITE setLineNumberWidth NOTIFY layoutChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)

public:
    explicit FmDocumentViewVisual(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    QString text() const { return m_text; }
    QString fontFamily() const { return m_font.family(); }
    qreal fontPixelSize() const { return m_font.pixelSize(); }
    QColor textColor() const { return m_textColor; }
    QColor lineNumberColor() const { return m_lineNumberColor; }
    QColor selectionColor() const { return m_selectionColor; }
    QColor selectedTextColor() const { return m_selectedTextColor; }
    QColor gutterColor() const { return m_gutterColor; }
    QColor dividerColor() const { return m_dividerColor; }
    QVariantList styleRanges() const { return m_styleRangeValues; }
    QColor tokenColor1() const { return m_tokenColors[0]; }
    QColor tokenColor2() const { return m_tokenColors[1]; }
    QColor tokenColor3() const { return m_tokenColors[2]; }
    QColor tokenColor4() const { return m_tokenColors[3]; }
    bool wrap() const { return m_wrap; }
    bool showLineNumbers() const { return m_showLineNumbers; }
    qint64 firstLine() const { return m_firstLine; }
    qreal contentX() const { return m_contentX; }
    qreal contentY() const { return m_contentY; }
    qreal documentWidth() const { return m_documentWidth; }
    qreal documentHeight() const { return m_documentHeight; }
    qreal textPadding() const { return m_textPadding; }
    qreal lineNumberWidth() const { return m_lineNumberWidth; }
    QString selectedText() const;
    bool hasSelection() const { return m_selectionAnchor != m_selectionCursor; }

    void setText(const QString &value);
    void setFontFamily(const QString &value);
    void setFontPixelSize(qreal value);
    void setTextColor(const QColor &value);
    void setLineNumberColor(const QColor &value);
    void setSelectionColor(const QColor &value);
    void setSelectedTextColor(const QColor &value);
    void setGutterColor(const QColor &value);
    void setDividerColor(const QColor &value);
    void setStyleRanges(const QVariantList &value);
    void setTokenColor1(const QColor &value);
    void setTokenColor2(const QColor &value);
    void setTokenColor3(const QColor &value);
    void setTokenColor4(const QColor &value);
    void setWrap(bool value);
    void setShowLineNumbers(bool value);
    void setFirstLine(qint64 value);
    void setContentX(qreal value);
    void setContentY(qreal value);
    void setTextPadding(qreal value);
    void setLineNumberWidth(qreal value);

    Q_INVOKABLE void copySelection() const;
    Q_INVOKABLE void clearSelection();

signals:
    void documentChanged();
    void layoutChanged();
    void visualChanged();
    void viewportChanged();
    void documentGeometryChanged();
    void selectionChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct LogicalLine {
        qsizetype start = 0;
        qsizetype length = 0;
    };
    struct VisualRow {
        qsizetype start = 0;
        qsizetype length = 0;
        int logicalLine = 0;
        bool firstSegment = true;
        qreal width = 0;
    };
    struct StyleRange {
        qsizetype start = 0;
        qsizetype length = 0;
        int role = 0;
        bool bold = false;
        bool italic = false;
    };

    void rebuildLayout();
    qsizetype positionAt(const QPointF &point) const;
    QRectF rowTextRect(int rowIndex) const;
    qreal gutterWidth() const;

    QString m_text;
    QFont m_font;
    QColor m_textColor;
    QColor m_lineNumberColor;
    QColor m_selectionColor;
    QColor m_selectedTextColor;
    QColor m_gutterColor;
    QColor m_dividerColor;
    QVector<LogicalLine> m_lines;
    QVector<VisualRow> m_rows;
    QVariantList m_styleRangeValues;
    QVector<StyleRange> m_styleRanges;
    QColor m_tokenColors[4];
    bool m_wrap = false;
    bool m_showLineNumbers = true;
    qint64 m_firstLine = 1;
    qreal m_contentX = 0;
    qreal m_contentY = 0;
    qreal m_documentWidth = 0;
    qreal m_documentHeight = 0;
    qreal m_textPadding = 24;
    qreal m_lineNumberWidth = 45;
    qreal m_characterWidth = 8;
    qreal m_lineHeight = 18;
    qsizetype m_selectionAnchor = 0;
    qsizetype m_selectionCursor = 0;
};
