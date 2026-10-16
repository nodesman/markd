#pragma once

#include <QWidget>
#include <QListView>
#include <QStandardItemModel>
#include <QLabel>
#include <vector>
#include "parser/TocItem.h"

class TocWidget : public QWidget {
    Q_OBJECT

public:
#if defined(Q_OS_WIN)
    static constexpr bool kDefaultAccentBar = true;
#else
    static constexpr bool kDefaultAccentBar = false;
#endif
    explicit TocWidget(QWidget* parent = nullptr, bool drawAccentBar = kDefaultAccentBar);
    ~TocWidget() override = default;

    void setHeadings(const std::vector<TocItem>& items);
    Q_INVOKABLE void setActiveHeading(const QString& headingId);
    QString activeHeadingId() const { return m_activeHeadingId; }
    QString activeHeadingTitle() const;
    int headingCount() const { return static_cast<int>(m_headings.size()); }
    const std::vector<TocItem>& headings() const { return m_headings; }

    QListView* view() const { return m_view; }
    QLabel* placeholderLabel() const { return m_placeholderLabel; }

signals:
    void headingActivated(const QString& id);

private slots:
    void onItemActivated(const QModelIndex& index);

private:
    void setupUI(bool drawAccentBar);

    QListView* m_view = nullptr;
    QStandardItemModel* m_model = nullptr;
    QLabel* m_placeholderLabel = nullptr;

    std::vector<TocItem> m_headings;
    QString m_activeHeadingId;
    bool m_isProgrammaticUpdate = false;
};
