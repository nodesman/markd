#include "ui/UpdateDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

UpdateDialog::UpdateDialog(QWidget* parent) : UpdateDialog(UpdateInfo{}, parent) {}

UpdateDialog::UpdateDialog(const UpdateInfo& info, QWidget* parent)
    : QDialog(parent), m_info(info)
{
    setObjectName(QStringLiteral("updateDialog"));
    setFixedSize(420, 280);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setupUi();
    updateUiForState();
}

UpdateDialog::~UpdateDialog() = default;

void UpdateDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 16, 24, 20);
    mainLayout->setSpacing(10);

    auto* topBarLayout = new QHBoxLayout();
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->addStretch();

    m_closeBtn = new QPushButton(QStringLiteral("✕"), this);
    m_closeBtn->setObjectName(QStringLiteral("dialogCloseButton"));
    m_closeBtn->setFixedSize(24, 24);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    topBarLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(topBarLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("updateStatusLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName(QStringLiteral("updateSubtitleLabel"));
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_subtitleLabel);

    m_commandContainer = new QWidget(this);
    m_commandContainer->setObjectName(QStringLiteral("updateCommandBox"));
    auto* cmdLayout = new QHBoxLayout(m_commandContainer);
    cmdLayout->setContentsMargins(12, 8, 12, 8);
    cmdLayout->setSpacing(6);

    m_promptLabel = new QLabel(QStringLiteral("$"), m_commandContainer);
    m_promptLabel->setObjectName(QStringLiteral("updatePromptLabel"));
    cmdLayout->addWidget(m_promptLabel);

    m_commandLabel = new QLabel(m_commandContainer);
    m_commandLabel->setObjectName(QStringLiteral("updateCommandLabel"));
    cmdLayout->addWidget(m_commandLabel);
    cmdLayout->addStretch();
    mainLayout->addWidget(m_commandContainer);

    m_copiedBadge = new QLabel(QStringLiteral("Copied to clipboard"), this);
    m_copiedBadge->setObjectName(QStringLiteral("copiedBadge"));
    m_copiedBadge->setAlignment(Qt::AlignCenter);
    m_copiedBadge->setVisible(false);
    mainLayout->addWidget(m_copiedBadge);

    mainLayout->addStretch();

    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_secondaryBtn = new QPushButton(QStringLiteral("Close"), this);
    m_secondaryBtn->setObjectName(QStringLiteral("dialogSecondaryButton"));
    m_secondaryBtn->setFixedHeight(32);
    connect(m_secondaryBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_secondaryBtn);

    m_actionBtn = new QPushButton(this);
    m_actionBtn->setObjectName(QStringLiteral("dialogPrimaryButton"));
    m_actionBtn->setFixedHeight(32);
    connect(m_actionBtn, &QPushButton::clicked, this, &UpdateDialog::triggerPrimaryAction);
    btnLayout->addWidget(m_actionBtn);

    mainLayout->addLayout(btnLayout);

    m_badgeTimer = new QTimer(this);
    m_badgeTimer->setSingleShot(true);
    connect(m_badgeTimer, &QTimer::timeout, this, [this]() {
        m_copiedBadge->setVisible(false);
    });
}

void UpdateDialog::setUpdateInfo(const UpdateInfo& info) {
    m_info = info;
    updateUiForState();
}

void UpdateDialog::updateUiForState() {
    if (m_info.state == UpdateState::Checking) {
        m_statusLabel->setText(QStringLiteral("Checking for updates…"));
        m_subtitleLabel->setText(QStringLiteral("Contacting updates.markd.dev…"));
        m_commandContainer->setVisible(false);
        m_secondaryBtn->setVisible(true);
        m_secondaryBtn->setText(QStringLiteral("Cancel"));
        m_actionBtn->setVisible(false);
        return;
    }

    m_actionBtn->setVisible(true);
    m_secondaryBtn->setText(QStringLiteral("Close"));

    if (m_info.state == UpdateState::UpToDate) {
        m_statusLabel->setText(QStringLiteral("✓ Markd is up to date"));
        m_subtitleLabel->setText(QStringLiteral("%1 is currently the newest version.").arg(m_info.currentVersion));
        m_commandContainer->setVisible(false);
        m_secondaryBtn->setVisible(false);
        m_actionBtn->setText(QStringLiteral("OK"));
        return;
    }

    if (m_info.state == UpdateState::CheckFailed) {
        m_statusLabel->setText(m_info.errorMessage.isEmpty()
            ? QStringLiteral("Unable to check for updates.")
            : m_info.errorMessage);
        m_subtitleLabel->setText(QStringLiteral("Check your internet connection and try again."));
        m_commandContainer->setVisible(false);
        m_secondaryBtn->setVisible(true);
        m_actionBtn->setText(QStringLiteral("Retry"));
        return;
    }

    m_statusLabel->setText(QStringLiteral("Update Available: %1").arg(m_info.latestVersion));
    m_subtitleLabel->setText(QStringLiteral("Current version: %1").arg(m_info.currentVersion));
    m_secondaryBtn->setVisible(true);

    if (m_info.provenance == InstallationProvenance::StandaloneBinary) {
        m_commandContainer->setVisible(false);
        m_actionBtn->setText(QStringLiteral("Download Update"));
        return;
    }

    m_commandContainer->setVisible(true);
    m_commandLabel->setText(m_info.upgradeCommand);
    m_actionBtn->setText(QStringLiteral("Copy Command"));
}

void UpdateDialog::copyCommand() {
    auto* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(m_info.upgradeCommand);
    }
    m_copiedBadge->setVisible(true);
    m_badgeTimer->start(2000);
}

void UpdateDialog::triggerPrimaryAction() {
    if (m_info.state == UpdateState::UpToDate) {
        accept();
        return;
    }
    if (m_info.state == UpdateState::CheckFailed) {
        emit retryRequested();
        return;
    }
    if (m_info.provenance == InstallationProvenance::StandaloneBinary) {
        if (!m_info.downloadUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(m_info.downloadUrl));
        }
        return;
    }
    copyCommand();
}

void UpdateDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void UpdateDialog::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}
