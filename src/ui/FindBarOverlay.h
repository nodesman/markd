#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QPair>
#include <QString>

class FindBarOverlay : public QWidget {
    Q_OBJECT

public:
    explicit FindBarOverlay(QWidget* parent = nullptr);
    ~FindBarOverlay() override = default;

    void activate(const QString& initialQuery = QString());
    void updateMatchCount(int activeMatch, int totalMatches);
    void clearSearch();

    static QPair<int, int> executeEditorSearch(
        QPlainTextEdit* editor,
        const QString& text,
        bool forward = true,
        bool caseSensitive = false,
        bool rewindToSelectionStart = false,
        bool moveCursor = true);

    QLineEdit* findInput() const { return m_findInput; }
    QLabel* matchCountLabel() const { return m_matchCountLabel; }
    QPushButton* prevButton() const { return m_prevBtn; }
    QPushButton* nextButton() const { return m_nextBtn; }
    QPushButton* closeButton() const { return m_closeBtn; }

    QString query() const;
    QString matchCountText() const;
    int activeMatch() const { return m_activeMatch; }
    int totalMatches() const { return m_totalMatches; }

signals:
    void queryChanged(const QString& text);
    void findNextRequested();
    void findPreviousRequested();
    void closeRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void configureInput();
    void configureButtons();
    void refreshCounterStyle();
    static bool stepEditorCursor(
        QPlainTextEdit* editor,
        const QString& text,
        bool forward,
        bool caseSensitive,
        bool rewindToSelectionStart);

    QLineEdit* m_findInput = nullptr;
    QLabel* m_matchCountLabel = nullptr;
    QPushButton* m_prevBtn = nullptr;
    QPushButton* m_nextBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    int m_activeMatch = 0;
    int m_totalMatches = 0;
};
