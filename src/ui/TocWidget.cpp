#include "ui/TocWidget.h"
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QItemSelectionModel>

class TocItemDelegate : public QStyledItemDelegate {
public:
    explicit TocItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        int level = index.data(Qt::UserRole + 2).toInt();
        if (level < 1) level = 1;
        QString title = index.data(Qt::DisplayRole).toString();

        bool isSelected = (option.state & QStyle::State_Selected);
        bool isHovered = (option.state & QStyle::State_MouseOver);

        // 1. Draw rounded row background
        QRect rowRect = option.rect.adjusted(4, 1, -4, -1);
        if (isSelected) {
            painter->setBrush(QColor("#161b22"));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(rowRect, 4, 4);
        } else if (isHovered) {
            painter->setBrush(QColor(255, 255, 255, 12));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(rowRect, 4, 4);
        }

        // 2. Indentation: (level - 1) * 12px + 12px base margin
        int indent = (level - 1) * 12 + 12;
        QRect textRect = option.rect.adjusted(indent, 0, -8, 0);

        // 3. Typography
        QFont font = option.font;
        font.setPointSize(12);
        if (level == 1) {
            font.setWeight(QFont::DemiBold);
        } else {
            font.setWeight(QFont::Normal);
        }
        painter->setFont(font);

        if (isSelected) {
            painter->setPen(QColor("#f0f6fc"));
        } else if (isHovered) {
            painter->setPen(QColor("#f0f6fc"));
        } else {
            painter->setPen(QColor("#8b949e"));
        }

        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, title);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        return QSize(option.rect.width(), 26);
    }
};

TocWidget::TocWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void TocWidget::setupUI()
{
    setObjectName("tocSidebar");
    setAccessibleName(tr("Table of Contents"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(0);

    m_view = new QListView(this);
    m_view->setObjectName("tocView");
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setMouseTracking(true);
    m_view->setAttribute(Qt::WA_Hover, true);

    m_model = new QStandardItemModel(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(new TocItemDelegate(this));

    m_placeholderLabel = new QLabel(tr("No headings"), this);
    m_placeholderLabel->setObjectName("tocPlaceholderLabel");
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setWordWrap(true);
    m_placeholderLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(m_view);
    layout->addWidget(m_placeholderLabel);

    // Initial state: empty placeholder visible
    m_placeholderLabel->setVisible(true);
    m_view->setVisible(false);

    connect(m_view, &QListView::clicked, this, &TocWidget::onItemActivated);
    connect(m_view, &QListView::activated, this, &TocWidget::onItemActivated);
}

void TocWidget::setHeadings(const std::vector<TocItem>& items)
{
    m_headings = items;
    m_model->clear();

    if (items.empty()) {
        m_placeholderLabel->setVisible(true);
        m_view->setVisible(false);
        m_activeHeadingId.clear();
        return;
    }

    m_placeholderLabel->setVisible(false);
    m_view->setVisible(true);

    for (const auto& item : items) {
        auto* stdItem = new QStandardItem(item.title);
        stdItem->setData(QVariant::fromValue(item), Qt::UserRole);
        stdItem->setData(item.id, Qt::UserRole + 1);
        stdItem->setData(item.level, Qt::UserRole + 2);
        stdItem->setEditable(false);
        m_model->appendRow(stdItem);
    }

    // Re-highlight active heading if still present
    if (!m_activeHeadingId.isEmpty()) {
        setActiveHeading(m_activeHeadingId);
    }
}

void TocWidget::setActiveHeading(const QString& headingId)
{
    if (headingId.isEmpty()) return;

    // Search model for matching heading ID
    QModelIndex targetIndex;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QModelIndex idx = m_model->index(row, 0);
        if (idx.data(Qt::UserRole + 1).toString() == headingId) {
            targetIndex = idx;
            break;
        }
    }

    if (!targetIndex.isValid()) {
        // Unknown heading ID safely no-ops without crashing or clearing selection
        return;
    }

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
