#include "ui/TocWidget.h"
#include "ui/TocHeaderWidget.h"
#include "ui/TocItemDelegate.h"
#include <QVBoxLayout>
#include <QItemSelectionModel>
#include <algorithm>

TocWidget::TocWidget(QWidget* parent, bool drawAccentBar)
    : QWidget(parent)
{
    setupUI(drawAccentBar);
}

void TocWidget::setupUI(bool drawAccentBar)
{
    setObjectName(QStringLiteral("tocSidebar"));
    setAccessibleName(tr("Table of Contents"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_headerWidget = new TocHeaderWidget(this, this);
    layout->addWidget(m_headerWidget);

    m_view = new QListView(this);
    m_view->setObjectName(QStringLiteral("tocView"));
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setMouseTracking(true);
    m_view->setAttribute(Qt::WA_Hover, true);

    m_model = new QStandardItemModel(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(new TocItemDelegate(this, drawAccentBar));

    m_placeholderLabel = new QLabel(tr("No headings"), this);
    m_placeholderLabel->setObjectName(QStringLiteral("tocPlaceholderLabel"));
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setWordWrap(true);
    m_placeholderLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_collapsedSpacer = new QWidget(this);
    m_collapsedSpacer->setObjectName(QStringLiteral("tocCollapsedSpacer"));
    m_collapsedSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_collapsedSpacer->setVisible(false);

    layout->addWidget(m_view, 1);
    layout->addWidget(m_placeholderLabel, 1);
    layout->addWidget(m_collapsedSpacer, 1);

    m_placeholderLabel->setVisible(true);
    m_view->setVisible(false);

    connect(m_view, &QListView::clicked, this, &TocWidget::onItemActivated);
    connect(m_view, &QListView::activated, this, &TocWidget::onItemActivated);
}

void TocWidget::populateModel(const std::vector<TocItem>& items)
{
    for (const auto& item : items) {
        auto* stdItem = new QStandardItem(item.title);
        stdItem->setData(QVariant::fromValue(item), Qt::UserRole);
        stdItem->setData(item.id, Qt::UserRole + 1);
        stdItem->setData(item.level, Qt::UserRole + 2);
        stdItem->setEditable(false);
        m_model->appendRow(stdItem);
    }
    if (!m_activeHeadingId.isEmpty()) {
        setActiveHeading(m_activeHeadingId);
    }
}

void TocWidget::syncContentVisibility()
{
    m_collapsedSpacer->setVisible(m_isSectionCollapsed);
    if (m_isSectionCollapsed) {
        m_view->setVisible(false);
        m_placeholderLabel->setVisible(false);
        return;
    }
    const bool empty = m_headings.empty();
    m_placeholderLabel->setVisible(empty);
    m_view->setVisible(!empty);
}

void TocWidget::setHeadings(const std::vector<TocItem>& items)
{
    m_headings = items;
    m_model->clear();
    if (items.empty()) {
        m_activeHeadingId.clear();
        syncContentVisibility();
        return;
    }
    populateModel(items);
    syncContentVisibility();
}

void TocWidget::toggleSection()
{
    setSectionCollapsed(!m_isSectionCollapsed);
}

void TocWidget::setSectionCollapsed(bool collapsed)
{
    m_isSectionCollapsed = collapsed;
    if (m_headerWidget && m_headerWidget->caretLabel()) {
        m_headerWidget->caretLabel()->setText(m_isSectionCollapsed ? QString::fromUtf8("›") : QString::fromUtf8("⌵"));
    }
    syncContentVisibility();
}

void TocWidget::setActiveHeading(const QString& headingId)
{
    if (headingId.isEmpty()) return;

    QModelIndex targetIndex;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QModelIndex idx = m_model->index(row, 0);
        if (idx.data(Qt::UserRole + 1).toString() == headingId) {
            targetIndex = idx;
            break;
        }
    }

    if (!targetIndex.isValid()) return;

    m_activeHeadingId = headingId;
    m_isProgrammaticUpdate = true;
    m_view->selectionModel()->setCurrentIndex(
        targetIndex,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
    );
    m_view->scrollTo(targetIndex, QAbstractItemView::EnsureVisible);
    m_isProgrammaticUpdate = false;
}

void TocWidget::onItemActivated(const QModelIndex& index)
{
    if (m_isProgrammaticUpdate || !index.isValid()) return;

    QString id = index.data(Qt::UserRole + 1).toString();
    if (!id.isEmpty()) {
        m_activeHeadingId = id;
        emit headingActivated(id);
    }
}

QString TocWidget::activeHeadingTitle() const
{
    if (m_activeHeadingId.isEmpty()) return {};
    auto it = std::find_if(m_headings.begin(), m_headings.end(), [this](const TocItem& item) {
        return item.id == m_activeHeadingId;
    });
    if (it != m_headings.end()) {
        return it->title;
    }
    return {};
}
