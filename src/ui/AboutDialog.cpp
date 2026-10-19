#include "ui/AboutDialog.h"
#include "core/Version.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("aboutDialog"));
    setFixedSize(340, 260);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setupUi();
}

AboutDialog::~AboutDialog() = default;

void AboutDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 18);
    mainLayout->setSpacing(8);

    auto* topBarLayout = new QHBoxLayout();
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->addStretch();

    m_closeBtn = new QPushButton(QStringLiteral("✕"), this);
    m_closeBtn->setObjectName(QStringLiteral("dialogCloseButton"));
    m_closeBtn->setFixedSize(24, 24);
    connect(m_closeBtn, &QPushButton::clicked, this, &AboutDialog::dismiss);
    topBarLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(topBarLayout);

    m_logoLabel = new QLabel(this);
    m_logoLabel->setObjectName(QStringLiteral("monolithLogoLabel"));
    m_logoLabel->setAlignment(Qt::AlignCenter);
    const QPixmap logoPix(QStringLiteral(":/branding/markd_glyph_transparent.png"));
    if (!logoPix.isNull()) {
        m_logoLabel->setPixmap(logoPix.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    mainLayout->addWidget(m_logoLabel);

    auto* titleLayout = new QHBoxLayout();
    titleLayout->setAlignment(Qt::AlignCenter);
    titleLayout->setSpacing(6);

    m_titleLabel = new QLabel(QStringLiteral("Markd"), this);
    m_titleLabel->setObjectName(QStringLiteral("aboutTitleLabel"));
    titleLayout->addWidget(m_titleLabel);

    m_versionLabel = new QLabel(QStringLiteral("v%1").arg(MARKD_VERSION_STR), this);
    m_versionLabel->setObjectName(QStringLiteral("aboutVersionLabel"));
    titleLayout->addWidget(m_versionLabel);
    mainLayout->addLayout(titleLayout);

    m_inscriptionLabel = new QLabel(QString::fromUtf8("மேற்கடல் ஆம்ஸ்டர்டாமில் வடித்த அறம் 🇳🇱"), this);
    m_inscriptionLabel->setObjectName(QStringLiteral("tamilInscriptionLabel"));
    m_inscriptionLabel->setAlignment(Qt::AlignCenter);
    m_inscriptionLabel->setToolTip(QString::fromUtf8(
        "Aram, shaped in Amsterdam by the western sea.\n"
        "(Aram / அறம்: Rightful conduct, duty, and architectural integrity)"
    ));
    mainLayout->addWidget(m_inscriptionLabel);

    mainLayout->addSpacing(4);

    m_checkUpdatesBtn = new QPushButton(QStringLiteral("Check for Updates"), this);
    m_checkUpdatesBtn->setObjectName(QStringLiteral("checkUpdatesButton"));
    m_checkUpdatesBtn->setFixedHeight(32);
    connect(m_checkUpdatesBtn, &QPushButton::clicked, this, [this]() {
        emit checkUpdatesRequested();
    });
    mainLayout->addWidget(m_checkUpdatesBtn);

    m_footerLabel = new QLabel(QStringLiteral("aramudan.dev/markd • © Raj Aramudan"), this);
    m_footerLabel->setObjectName(QStringLiteral("aboutFooterLabel"));
    m_footerLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_footerLabel);
}

bool AboutDialog::hasValidLogo() const {
    if (!m_logoLabel) return false;
    return !m_logoLabel->pixmap().isNull();
}

QString AboutDialog::inscriptionText() const {
    if (!m_inscriptionLabel) return QString();
    return m_inscriptionLabel->text();
}

QString AboutDialog::inscriptionToolTip() const {
    if (!m_inscriptionLabel) return QString();
    return m_inscriptionLabel->toolTip();
}

QString AboutDialog::versionText() const {
    if (!m_versionLabel) return QString();
    return m_versionLabel->text();
}

void AboutDialog::dismiss() {
    reject();
}

void AboutDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void AboutDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void AboutDialog::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}
