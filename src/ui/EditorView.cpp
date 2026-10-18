#include "ui/EditorView.h"
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <algorithm>

LineNumberArea::LineNumberArea(EditorView* editor)
    : QWidget(editor)
    , m_editor(editor)
{
    setObjectName(QStringLiteral("lineNumberArea"));
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent* event)
{
    m_editor->lineNumberAreaPaintEvent(event);
}

EditorView::EditorView(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
{
    configureTypography();
    connect(this, &EditorView::blockCountChanged, this, &EditorView::updateLineNumberAreaWidth);
    connect(this, &EditorView::updateRequest, this, &EditorView::updateLineNumberArea);
    connect(this, &EditorView::cursorPositionChanged, this, &EditorView::highlightCurrentLine);
    updateLineNumberAreaWidth();
    highlightCurrentLine();
}

void EditorView::configureTypography()
{
    QFont monoFont(QStringLiteral("SF Mono"));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setFixedPitch(true);
    monoFont.setPointSize(13);
    setFont(monoFont);

    document()->setDocumentMargin(16);

    QPalette p = palette();
    p.setColor(QPalette::Base, QColor(QStringLiteral("#050505")));
    p.setColor(QPalette::Text, QColor(QStringLiteral("#f0f6fc")));
    setPalette(p);
}

int EditorView::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = std::max(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return digits * fontMetrics().horizontalAdvance('9') + 24;
}

void EditorView::updateLineNumberAreaWidth()
{
    const int newWidth = lineNumberAreaWidth();
    if (viewportMargins().left() != newWidth) {
        setViewportMargins(newWidth, 0, 0, 0);
    }
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), newWidth, cr.height()));
}

void EditorView::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy != 0) {
        m_lineNumberArea->scroll(0, dy);
        return;
    }
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}

void EditorView::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void EditorView::changeEvent(QEvent* event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        updateLineNumberAreaWidth();
    }
}

void EditorView::setLineHighlightEnabled(bool enabled)
{
    m_lineHighlightEnabled = enabled;
    highlightCurrentLine();
}

void EditorView::setSearchSelections(const QList<QTextEdit::ExtraSelection>& selections)
{
    m_searchSelections = selections;
    highlightCurrentLine();
}

void EditorView::clearSearchSelections()
{
    m_searchSelections.clear();
    highlightCurrentLine();
}

void EditorView::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> combined = m_searchSelections;
    if (!isReadOnly() && m_lineHighlightEnabled) {
        QTextEdit::ExtraSelection s;
        s.format.setBackground(QColor(QStringLiteral("#161b22")));
        s.format.setProperty(QTextFormat::FullWidthSelection, true);
        s.cursor = textCursor();
        s.cursor.clearSelection();
        combined.prepend(s);
    }
    QPlainTextEdit::setExtraSelections(combined);
    m_lineNumberArea->update();
}

void EditorView::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(QStringLiteral("#0d1117")));
    painter.setPen(QColor(QStringLiteral("#1e242c")));
    const int rightX = m_lineNumberArea->width() - 1;
    painter.drawLine(rightX, event->rect().top(), rightX, event->rect().bottom());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());
    const int activeBlockNumber = textCursor().blockNumber();
    const int textRight = m_lineNumberArea->width() - 8;

    while (block.isValid() && top <= event->rect().bottom()) {
        paintBlockNumber(&painter, block, blockNumber, activeBlockNumber, top, bottom, textRight, event->rect());
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void EditorView::paintBlockNumber(
    QPainter* painter,
    const QTextBlock& block,
    int blockNumber,
    int activeBlockNumber,
    int top,
    int bottom,
    int textRight,
    const QRect& clipRect)
{
    if (!block.isVisible() || bottom < clipRect.top()) return;
    const bool isActive = (blockNumber == activeBlockNumber);
    painter->setPen(QColor(isActive ? QStringLiteral("#f0f6fc") : QStringLiteral("#484f58")));
    QFont font = this->font();
    font.setWeight(isActive ? QFont::DemiBold : QFont::Normal);
    painter->setFont(font);
    const QString number = QString::number(blockNumber + 1);
    const int blockHeight = static_cast<int>(blockBoundingRect(block).height());
    painter->drawText(0, top, textRight, blockHeight, Qt::AlignRight | Qt::AlignVCenter, number);
}
