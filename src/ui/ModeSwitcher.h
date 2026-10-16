#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

class ModeSwitcher : public QWidget {
    Q_OBJECT

public:
    enum class DocumentMode {
        Edit,
        Preview
    };

    explicit ModeSwitcher(QWidget* parent = nullptr);
    ~ModeSwitcher() override = default;

    DocumentMode currentMode() const { return m_currentMode; }
    QSize sizeHint() const override { return QSize(154, 30); }

public slots:
    void setMode(DocumentMode mode);
    void toggleMode();

signals:
    void modeChanged(DocumentMode mode);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void updateSegmentStyles();

    DocumentMode m_currentMode = DocumentMode::Edit;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_previewBtn = nullptr;
};
