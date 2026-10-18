#pragma once

#include <QWidget>
#include <QLabel>

class TocWidget;

class TocHeaderWidget : public QWidget {
    Q_OBJECT

public:
    explicit TocHeaderWidget(TocWidget* owner, QWidget* parent = nullptr);
    ~TocHeaderWidget() override = default;

    QLabel* iconLabel() const { return m_iconLabel; }
    QLabel* caretLabel() const { return m_caretLabel; }
    QLabel* titleLabel() const { return m_titleLabel; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupUI();

    TocWidget* m_owner = nullptr;
    QLabel* m_iconLabel = nullptr;
    QLabel* m_caretLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
};
