#pragma once

#include <QDialog>
#include <QPoint>

class QLabel;
class QPushButton;

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog() override;

    bool hasValidLogo() const;
    QString inscriptionText() const;
    QString inscriptionToolTip() const;
    QString versionText() const;
    QString footerText() const;
    QString silentUpdateStatusText() const;
    void setSilentUpdateStatus(const QString& statusText);
    void dismiss();

signals:
    void checkUpdatesRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void setupUi();

    QLabel* m_logoLabel{nullptr};
    QLabel* m_titleLabel{nullptr};
    QLabel* m_versionLabel{nullptr};
    QLabel* m_updateStatusLabel{nullptr};
    QLabel* m_inscriptionLabel{nullptr};
    QPushButton* m_checkUpdatesBtn{nullptr};
    QLabel* m_footerLabel{nullptr};
    QPushButton* m_closeBtn{nullptr};
    QPoint m_dragPosition;
    bool m_dragging{false};
};
