#pragma once

#include <QStyledItemDelegate>
#include <QPainter>
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
        return QSize(option.rect.width(), 28);
    }

private:
    bool m_drawAccentBar;

    void drawRowBackground(QPainter* painter, const QRect& rowRect, bool isSelected, bool isHovered) const {
        if (!isSelected && !isHovered) return;
        int selectedAlpha = m_drawAccentBar ? 10 : 20;
        painter->setBrush(QColor(255, 255, 255, isSelected ? selectedAlpha : 10));
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
