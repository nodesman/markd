#pragma once

#include <QDialog>
#include <QPoint>
#include <QLabel>
#include <QPushButton>
#include <QKeyEvent>
#include "core/UpdateChecker.h"

class QWidget;
class QTimer;

class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget* parent = nullptr);
    explicit UpdateDialog(const UpdateInfo& info, QWidget* parent = nullptr);
    ~UpdateDialog() override;

    void setUpdateInfo(const UpdateInfo& info);
    UpdateInfo updateInfo() const { return m_info; }

    QString statusMessage() const { return m_statusLabel ? m_statusLabel->text() : QString(); }
    QString actionButtonText() const { return m_actionBtn ? m_actionBtn->text() : QString(); }
    QString commandText() const { return m_commandLabel ? m_commandLabel->text() : QString(); }
    void triggerPrimaryAction();
    bool isCopiedBadgeVisible() const { return m_copiedBadge && m_copiedBadge->isVisible(); }
    QString copiedBadgeText() const { return m_copiedBadge ? m_copiedBadge->text() : QString(); }
    bool hasSquareBracketsInUiText() const {
        const QString all = statusMessage() + actionButtonText() + commandText();
        return all.contains(QLatin1Char('[')) || all.contains(QLatin1Char(']'));
    }

signals:
    void retryRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            reject();
            event->accept();
            return;
        }
        QDialog::keyPressEvent(event);
    }
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void setupUi();
    void updateUiForState();
    void copyCommand();

    UpdateInfo m_info;
    QLabel* m_statusLabel{nullptr};
    QLabel* m_subtitleLabel{nullptr};
    QWidget* m_commandContainer{nullptr};
    QLabel* m_promptLabel{nullptr};
    QLabel* m_commandLabel{nullptr};
    QPushButton* m_actionBtn{nullptr};
    QPushButton* m_secondaryBtn{nullptr};
    QLabel* m_copiedBadge{nullptr};
    QPushButton* m_closeBtn{nullptr};
    QTimer* m_badgeTimer{nullptr};
    QPoint m_dragPosition;
};
