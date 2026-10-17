#include "ui/TocWidget.h"
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QItemSelectionModel>
#include <algorithm>

class TocItemDelegate : public QStyledItemDelegate {
public:
#if defined(Q_OS_WIN)
    static constexpr bool kDefaultAccentBar = true;
#else
    static constexpr bool kDefaultAccentBar = false;
#endif
    explicit TocItemDelegate(QObject* parent = nullptr, bool drawAccentBar = kDefaultAccentBar)
        : QStyledItemDelegate(parent), m_drawAccentBar(drawAccentBar) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        int level = std::max(1, index.data(Qt::UserRole + 2).toInt());
        QString title = index.data(Qt::DisplayRole).toString();
        bool isSelected = (option.state & QStyle::State_Selected);
        bool isHovered = (option.state & QStyle::State_MouseOver);
        QRect rowRect = option.rect.adjusted(4, 1, -4, -1);
        drawRowBackground(painter, rowRect, isSelected, isHovered);
        drawWindowsAccentIndicator(painter, rowRect, isSelected);
        drawRowText(painter, option, title, level, isSelected, isHovered);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        return QSize(option.rect.width(), 26);
    }

private:
    bool m_drawAccentBar;

    void drawRowBackground(QPainter* painter, const QRect& rowRect, bool isSelected, bool isHovered) const {
        if (!isSelected && !isHovered) return;
        int selectedAlpha = m_drawAccentBar ? 10 : 20;
        painter->setBrush(QColor(255, 255, 255, isSelected ? selectedAlpha : 12));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(rowRect, 6, 6);
    }

    void drawWindowsAccentIndicator(QPainter* painter, const QRect& rowRect, bool isSelected) const {
        if (!m_drawAccentBar || !isSelected) return;
        QRect accentBar(rowRect.left() + 2, rowRect.top() + 4, 3, rowRect.height() - 8);
        painter->setBrush(QColor("#58a6ff"));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(accentBar, 1.5, 1.5);
    }

    void drawRowText(QPainter* painter, const QStyleOptionViewItem& option, const QString& title, int level, bool isSelected, bool isHovered) const {
        int indent = (level - 1) * 12 + 12;
        QFont font = option.font;
        font.setPointSize(12);
        font.setWeight((level == 1 || isSelected) ? QFont::DemiBold : QFont::Normal);
        painter->setFont(font);
        painter->setPen(QColor((isSelected || isHovered) ? "#f0f6fc" : "#8b949e"));
        painter->drawText(option.rect.adjusted(indent, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, title);
    }
};

TocWidget::TocWidget(QWidget* parent, bool drawAccentBar)
    : QWidget(parent)
{
    setupUI(drawAccentBar);
}

void TocWidget::setupUI(bool drawAccentBar)
{
    setObjectName("tocSidebar");
    setAccessibleName(tr("Table of Contents"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 12, 0, 8);
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
    m_view->setItemDelegate(new TocItemDelegate(this, drawAccentBar));

    m_placeholderLabel = new QLabel(tr("No headings"), this);
    m_placeholderLabel->setObjectName("tocPlaceholderLabel");
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setWordWrap(true);
    m_placeholderLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(m_view);
    layout->addWidget(m_placeholderLabel);

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

    if (!m_activeHeadingId.isEmpty()) {
        setActiveHeading(m_activeHeadingId);
    }
}

void TocWidget::setActiveHeading(const QString& headingId)
{
    if (headingId.isEmpty()) return;

    QModelIndex targetIndex;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QModelIndex idx = m_model->index(row, 0);
        if (idx.data(Qt::UserRole + 1).toString() == headingId) { targetIndex = idx; break; }
    }

    if (!targetIndex.isValid()) {
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
