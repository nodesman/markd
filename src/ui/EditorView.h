#pragma once

#include <QPlainTextEdit>
#include <QWidget>
#include <QList>
#include <QTextEdit>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QMargins>

class EditorView;

class LineNumberArea : public QWidget {
    Q_OBJECT
public:
    explicit LineNumberArea(EditorView* editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    EditorView* m_editor = nullptr;
};

class EditorView : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit EditorView(QWidget* parent = nullptr);
    ~EditorView() override = default;

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);
    LineNumberArea* lineNumberArea() const { return m_lineNumberArea; }
    QMargins viewportMargins() const { return QPlainTextEdit::viewportMargins(); }
    using QPlainTextEdit::blockBoundingGeometry;
    using QPlainTextEdit::contentOffset;

    void setSearchSelections(const QList<QTextEdit::ExtraSelection>& selections);
    void clearSearchSelections();
    QList<QTextEdit::ExtraSelection> searchSelections() const { return m_searchSelections; }
    QList<QTextEdit::ExtraSelection> extraSelections() const { return m_searchSelections; }
    void setExtraSelections(const QList<QTextEdit::ExtraSelection>& selections) { setSearchSelections(selections); }
    QList<QTextEdit::ExtraSelection> allExtraSelections() const { return QPlainTextEdit::extraSelections(); }

    void setLineHighlightEnabled(bool enabled);
    bool isLineHighlightEnabled() const { return m_lineHighlightEnabled; }

public slots:
    void highlightCurrentLine();
    void updateLineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    void configureTypography();
    void paintBlockNumber(QPainter* painter, const QTextBlock& block, int blockNumber, int activeBlockNumber, int top, int bottom, int textRight, const QRect& clipRect);

    LineNumberArea* m_lineNumberArea = nullptr;
    QList<QTextEdit::ExtraSelection> m_searchSelections;
    bool m_lineHighlightEnabled = true;
};
