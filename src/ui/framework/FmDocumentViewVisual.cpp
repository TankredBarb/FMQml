#include "FmDocumentViewVisual.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTextCharFormat>
#include <QTextLayout>

FmDocumentViewVisual::FmDocumentViewVisual(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    m_font = QGuiApplication::font();
    m_font.setPixelSize(13);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setKeepMouseGrab(true);
    setFlag(ItemIsFocusScope, true);
    setAntialiasing(false);
    connect(this, &FmDocumentViewVisual::documentChanged,
            this, &FmDocumentViewVisual::rebuildLayout);
    connect(this, &FmDocumentViewVisual::layoutChanged,
            this, &FmDocumentViewVisual::rebuildLayout);
    connect(this, &FmDocumentViewVisual::visualChanged, this, &QQuickItem::update);
    connect(this, &FmDocumentViewVisual::viewportChanged, this, &QQuickItem::update);
}

qreal FmDocumentViewVisual::gutterWidth() const
{
    return m_showLineNumbers ? m_lineNumberWidth : 0.0;
}

void FmDocumentViewVisual::rebuildLayout()
{
    m_lines.clear();
    m_rows.clear();

    qsizetype start = 0;
    for (qsizetype index = 0; index <= m_text.size(); ++index) {
        if (index == m_text.size() || m_text.at(index) == QLatin1Char('\n')) {
            qsizetype length = index - start;
            if (length > 0 && m_text.at(start + length - 1) == QLatin1Char('\r'))
                --length;
            m_lines.append({start, length});
            start = index + 1;
        }
    }
    if (m_text.endsWith(QLatin1Char('\n')) && !m_lines.isEmpty())
        m_lines.removeLast();

    const QFontMetricsF metrics(m_font);
    m_characterWidth = qMax<qreal>(1.0, metrics.horizontalAdvance(QLatin1Char('M')));
    m_lineHeight = qMax<qreal>(1.0, qCeil(metrics.height() + 2.0));
    const qreal availableWidth = qMax<qreal>(m_characterWidth,
        width() - gutterWidth() - 2.0 * m_textPadding);
    qreal maximumLineWidth = 0;

    for (int lineIndex = 0; lineIndex < m_lines.size(); ++lineIndex) {
        const LogicalLine &line = m_lines.at(lineIndex);
        const QString lineText = m_text.mid(line.start, line.length);
        if (!m_wrap || line.length == 0) {
            const qreal rowWidth = metrics.horizontalAdvance(lineText);
            m_rows.append({line.start, line.length, lineIndex, true, rowWidth});
            maximumLineWidth = qMax(maximumLineWidth, rowWidth);
            continue;
        }
        QTextLayout layout(lineText, m_font);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        layout.beginLayout();
        bool firstSegment = true;
        while (true) {
            QTextLine textLine = layout.createLine();
            if (!textLine.isValid()) {
                break;
            }
            textLine.setLineWidth(availableWidth);
            m_rows.append({line.start + textLine.textStart(), textLine.textLength(),
                           lineIndex, firstSegment, textLine.naturalTextWidth()});
            firstSegment = false;
        }
        layout.endLayout();
    }

    m_documentWidth = m_wrap ? width()
        : gutterWidth() + 2.0 * m_textPadding + maximumLineWidth;
    m_documentHeight = 2.0 * m_textPadding + m_rows.size() * m_lineHeight;
    emit documentGeometryChanged();
    update();
}

QRectF FmDocumentViewVisual::rowTextRect(int rowIndex) const
{
    const qreal x = gutterWidth() + m_textPadding - m_contentX;
    const qreal y = m_textPadding + rowIndex * m_lineHeight - m_contentY;
    return QRectF(x, y, qMax<qreal>(0, width() - x), m_lineHeight);
}

void FmDocumentViewVisual::paint(QPainter *painter)
{
    if (m_rows.isEmpty() || width() <= 0 || height() <= 0)
        return;

    painter->setFont(m_font);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    if (m_showLineNumbers) {
        painter->fillRect(QRectF(0, 0, m_lineNumberWidth, height()), m_gutterColor);
        painter->fillRect(QRectF(m_lineNumberWidth - 1, 0, 1, height()), m_dividerColor);
    }
    const int firstRow = qBound(0, static_cast<int>(qFloor((m_contentY - m_textPadding) / m_lineHeight)),
                                m_rows.size() - 1);
    const int lastRow = qBound(firstRow,
        static_cast<int>(qCeil((m_contentY + height() - m_textPadding) / m_lineHeight)),
        m_rows.size() - 1);
    const qsizetype selectionStart = qMin(m_selectionAnchor, m_selectionCursor);
    const qsizetype selectionEnd = qMax(m_selectionAnchor, m_selectionCursor);

    for (int rowIndex = firstRow; rowIndex <= lastRow; ++rowIndex) {
        const VisualRow &row = m_rows.at(rowIndex);
        const QRectF textRect = rowTextRect(rowIndex);
        if (m_showLineNumbers && row.firstSegment) {
            painter->setPen(m_lineNumberColor);
            painter->drawText(QRectF(0, textRect.y(), m_lineNumberWidth - 8, m_lineHeight),
                              Qt::AlignRight | Qt::AlignVCenter,
                              QString::number(m_firstLine + row.logicalLine));
        }

        const qsizetype rowEnd = row.start + row.length;
        const qsizetype selectedStart = qMax(selectionStart, row.start);
        const qsizetype selectedEnd = qMin(selectionEnd, rowEnd);

        QList<QTextLayout::FormatRange> formats;
        formats.reserve(m_styleRanges.size() + (selectedStart < selectedEnd ? 1 : 0));
        for (const StyleRange &style : m_styleRanges) {
            const qsizetype styleStart = qMax(style.start, row.start);
            const qsizetype styleEnd = qMin(style.start + style.length, rowEnd);
            if (styleStart >= styleEnd) {
                continue;
            }
            QTextLayout::FormatRange format;
            format.start = static_cast<int>(styleStart - row.start);
            format.length = static_cast<int>(styleEnd - styleStart);
            format.format.setForeground(m_tokenColors[style.role - 1]);
            format.format.setFontWeight(style.bold ? QFont::Bold : QFont::Normal);
            format.format.setFontItalic(style.italic);
            formats.append(format);
        }
        if (selectedStart < selectedEnd) {
            QTextLayout::FormatRange selection;
            selection.start = static_cast<int>(selectedStart - row.start);
            selection.length = static_cast<int>(selectedEnd - selectedStart);
            selection.format.setBackground(m_selectionColor);
            selection.format.setForeground(m_selectedTextColor);
            formats.append(selection);
        }

        QTextLayout layout(m_text.mid(row.start, row.length), m_font);
        layout.setFormats(formats);
        layout.beginLayout();
        QTextLine line = layout.createLine();
        if (line.isValid()) {
            line.setLineWidth(qMax<qreal>(row.width, width()));
            line.setPosition(QPointF(0, 0));
        }
        layout.endLayout();
        painter->setPen(m_textColor);
        if (line.isValid()) {
            line.draw(painter, textRect.topLeft());
        }
    }
}

qsizetype FmDocumentViewVisual::positionAt(const QPointF &point) const
{
    if (m_rows.isEmpty())
        return 0;
    const int rowIndex = qBound(0,
        static_cast<int>(qFloor((point.y() + m_contentY - m_textPadding) / m_lineHeight)),
        m_rows.size() - 1);
    const VisualRow &row = m_rows.at(rowIndex);
    const qreal textX = qMax<qreal>(0, point.x() + m_contentX
        - gutterWidth() - m_textPadding);
    const QFontMetricsF metrics(m_font);
    qsizetype low = 0;
    qsizetype high = row.length;
    while (low < high) {
        const qsizetype middle = (low + high + 1) / 2;
        if (metrics.horizontalAdvance(m_text.mid(row.start, middle)) <= textX) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    const qsizetype column = low;
    return row.start + column;
}

void FmDocumentViewVisual::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus();
    m_selectionAnchor = m_selectionCursor = positionAt(event->position());
    emit selectionChanged();
    update();
    event->accept();
}

void FmDocumentViewVisual::mouseMoveEvent(QMouseEvent *event)
{
    m_selectionCursor = positionAt(event->position());
    emit selectionChanged();
    update();
    event->accept();
}

void FmDocumentViewVisual::mouseReleaseEvent(QMouseEvent *event)
{
    m_selectionCursor = positionAt(event->position());
    emit selectionChanged();
    update();
    event->accept();
}

void FmDocumentViewVisual::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy) && hasSelection()) {
        copySelection();
        event->accept();
        return;
    }
    QQuickPaintedItem::keyPressEvent(event);
}

QString FmDocumentViewVisual::selectedText() const
{
    const qsizetype start = qMin(m_selectionAnchor, m_selectionCursor);
    return m_text.mid(start, qAbs(m_selectionCursor - m_selectionAnchor));
}

void FmDocumentViewVisual::copySelection() const
{
    if (hasSelection())
        QGuiApplication::clipboard()->setText(selectedText());
}

void FmDocumentViewVisual::clearSelection()
{
    if (!hasSelection())
        return;
    m_selectionAnchor = m_selectionCursor = 0;
    emit selectionChanged();
    update();
}

void FmDocumentViewVisual::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width()))
        rebuildLayout();
}

#define FM_VALUE_SETTER(Name, Type, Member, Signal) \
    void FmDocumentViewVisual::Name(Type value) { if (Member == value) return; Member = value; emit Signal(); update(); }

void FmDocumentViewVisual::setText(const QString &value)
{
    if (m_text == value) return;
    m_text = value;
    m_selectionAnchor = m_selectionCursor = 0;
    emit selectionChanged();
    emit documentChanged();
}
void FmDocumentViewVisual::setFontFamily(const QString &value)
{
    if (m_font.family() == value) return;
    m_font.setFamily(value); emit layoutChanged();
}
void FmDocumentViewVisual::setFontPixelSize(qreal value)
{
    const int pixelSize = qMax(1, qRound(value));
    if (m_font.pixelSize() == pixelSize) return;
    m_font.setPixelSize(pixelSize); emit layoutChanged();
}
FM_VALUE_SETTER(setTextColor, const QColor &, m_textColor, visualChanged)
FM_VALUE_SETTER(setLineNumberColor, const QColor &, m_lineNumberColor, visualChanged)
FM_VALUE_SETTER(setSelectionColor, const QColor &, m_selectionColor, visualChanged)
FM_VALUE_SETTER(setSelectedTextColor, const QColor &, m_selectedTextColor, visualChanged)
FM_VALUE_SETTER(setGutterColor, const QColor &, m_gutterColor, visualChanged)
FM_VALUE_SETTER(setDividerColor, const QColor &, m_dividerColor, visualChanged)
FM_VALUE_SETTER(setTokenColor1, const QColor &, m_tokenColors[0], visualChanged)
FM_VALUE_SETTER(setTokenColor2, const QColor &, m_tokenColors[1], visualChanged)
FM_VALUE_SETTER(setTokenColor3, const QColor &, m_tokenColors[2], visualChanged)
FM_VALUE_SETTER(setTokenColor4, const QColor &, m_tokenColors[3], visualChanged)
FM_VALUE_SETTER(setWrap, bool, m_wrap, layoutChanged)
FM_VALUE_SETTER(setShowLineNumbers, bool, m_showLineNumbers, layoutChanged)
FM_VALUE_SETTER(setFirstLine, qint64, m_firstLine, visualChanged)
FM_VALUE_SETTER(setContentX, qreal, m_contentX, viewportChanged)
FM_VALUE_SETTER(setContentY, qreal, m_contentY, viewportChanged)
FM_VALUE_SETTER(setTextPadding, qreal, m_textPadding, layoutChanged)
FM_VALUE_SETTER(setLineNumberWidth, qreal, m_lineNumberWidth, layoutChanged)

void FmDocumentViewVisual::setStyleRanges(const QVariantList &value)
{
    if (m_styleRangeValues == value) {
        return;
    }
    m_styleRangeValues = value;
    m_styleRanges.clear();
    m_styleRanges.reserve(value.size());
    for (const QVariant &item : value) {
        const QVariantMap map = item.toMap();
        const qsizetype start = map.value(QStringLiteral("start")).toLongLong();
        const qsizetype length = map.value(QStringLiteral("length")).toLongLong();
        const int role = map.value(QStringLiteral("role")).toInt();
        if (start < 0 || length <= 0 || role < 1 || role > 4) {
            continue;
        }
        m_styleRanges.append({start, length, role,
                              map.value(QStringLiteral("bold")).toBool(),
                              map.value(QStringLiteral("italic")).toBool()});
    }
    emit visualChanged();
}

#undef FM_VALUE_SETTER
