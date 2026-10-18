#include "ui/FindBarOverlay.h"
#include "ui/EditorView.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QKeySequence>
#include <QStyle>
#include <QTextCursor>
#include <QTextDocument>
#include <QScrollBar>
#include <algorithm>

FindBarOverlay::FindBarOverlay(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("findBarOverlay"));
    setFixedSize(320, 34);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);
    configureInput();
    configureButtons();
    layout->addWidget(m_findInput, 1);
    layout->addWidget(m_matchCountLabel);
    layout->addWidget(m_prevBtn);
    layout->addWidget(m_nextBtn);
    layout->addWidget(m_closeBtn);
    hide();
}

void FindBarOverlay::configureInput()
{
    m_findInput = new QLineEdit(this);
    m_findInput->setObjectName(QStringLiteral("findInput"));
    m_findInput->setPlaceholderText(tr("Find in document…"));
    m_findInput->setAccessibleName(tr("Find in document"));
    m_findInput->setToolTip(tr("Find in document"));
    m_findInput->installEventFilter(this);

    m_matchCountLabel = new QLabel(this);
    m_matchCountLabel->setObjectName(QStringLiteral("findMatchCountLabel"));
    m_matchCountLabel->setAccessibleName(tr("Search match count"));
    m_matchCountLabel->setToolTip(tr("Search match count"));
    m_matchCountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(m_findInput, &QLineEdit::textChanged, this, &FindBarOverlay::queryChanged);
}

static QPushButton* createFindButton(const QString& glyph, const QString& objName, const QString& name, const QString& tip, bool enabled, QWidget* parent)
{
    auto* btn = new QPushButton(glyph, parent);
    btn->setObjectName(objName);
    btn->setFixedSize(22, 22);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setAccessibleName(name);
    btn->setToolTip(tip);
    btn->setEnabled(enabled);
    return btn;
}

void FindBarOverlay::configureButtons()
{
    m_prevBtn = createFindButton(QString::fromUtf8("▲"), QStringLiteral("findPrevButton"), tr("Previous Match"), tr("Previous Match (Shift+Enter)"), false, this);
    m_nextBtn = createFindButton(QString::fromUtf8("▼"), QStringLiteral("findNextButton"), tr("Next Match"), tr("Next Match (Enter)"), false, this);
    m_closeBtn = createFindButton(QString::fromUtf8("✕"), QStringLiteral("findCloseButton"), tr("Close Find Bar"), tr("Close Find Bar (Escape)"), true, this);
    connect(m_prevBtn, &QPushButton::clicked, this, &FindBarOverlay::findPreviousRequested);
    connect(m_nextBtn, &QPushButton::clicked, this, &FindBarOverlay::findNextRequested);
    connect(m_closeBtn, &QPushButton::clicked, this, &FindBarOverlay::closeRequested);
}

void FindBarOverlay::activate(const QString& initialQuery)
{
    show();
    raise();
    if (!initialQuery.isEmpty() && m_findInput->text() != initialQuery) {
        m_findInput->setText(initialQuery);
    }
    m_findInput->setFocus(Qt::ShortcutFocusReason);
    m_findInput->selectAll();
}

void FindBarOverlay::updateMatchCount(int activeMatch, int totalMatches)
{
    m_totalMatches = std::max(0, totalMatches);
    m_activeMatch = (m_totalMatches > 0) ? std::clamp(activeMatch, 1, m_totalMatches) : 0;
    const bool hasMatches = (m_totalMatches > 0);
    m_prevBtn->setEnabled(hasMatches);
    m_nextBtn->setEnabled(hasMatches);
    refreshCounterStyle();
}

void FindBarOverlay::refreshCounterStyle()
{
    const bool emptyQuery = query().isEmpty();
    const bool hasMatches = emptyQuery || (m_totalMatches > 0);
    const QString labelText = emptyQuery ? QString() : (hasMatches ? QStringLiteral("%1 of %2").arg(m_activeMatch).arg(m_totalMatches) : tr("0 matches"));
    m_matchCountLabel->setText(labelText);
    m_matchCountLabel->setProperty("hasMatches", hasMatches);
    m_matchCountLabel->style()->unpolish(m_matchCountLabel);
    m_matchCountLabel->style()->polish(m_matchCountLabel);
}

void FindBarOverlay::clearSearch()
{
    m_findInput->clear();
    updateMatchCount(0, 0);
}

QString FindBarOverlay::query() const { return m_findInput->text(); }
QString FindBarOverlay::matchCountText() const { return m_matchCountLabel->text(); }

bool FindBarOverlay::stepEditorCursor(QPlainTextEdit* editor, const QString& text, bool forward, bool caseSensitive, bool rewindToSelectionStart)
{
    const QTextCursor savedCursor = editor->textCursor();
    if (rewindToSelectionStart && savedCursor.hasSelection()) {
        QTextCursor rewindCursor = savedCursor;
        rewindCursor.setPosition(savedCursor.selectionStart());
        editor->setTextCursor(rewindCursor);
    }
    QTextDocument::FindFlags flags;
    if (!forward) flags |= QTextDocument::FindBackward;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    if (editor->find(text, flags)) return true;

    QTextCursor wrapCursor = editor->textCursor();
    wrapCursor.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
    editor->setTextCursor(wrapCursor);
    if (editor->find(text, flags)) return true;

    QTextCursor restoredCursor = savedCursor;
    restoredCursor.setPosition(savedCursor.hasSelection() ? savedCursor.selectionStart() : savedCursor.position());
    editor->setTextCursor(restoredCursor);
    return false;
}

QPair<int, int> FindBarOverlay::executeEditorSearch(QPlainTextEdit* editor, const QString& text, bool forward, bool caseSensitive, bool rewindToSelectionStart, bool moveCursor)
{
    if (!editor || text.isEmpty()) {
        if (auto* ev = qobject_cast<EditorView*>(editor)) {
            ev->clearSearchSelections();
            return {0, 0};
        }
        if (editor) editor->setExtraSelections({});
        return {0, 0};
    }
    const QTextCursor initialCursor = editor->textCursor();
    if (moveCursor) stepEditorCursor(editor, text, forward, caseSensitive, rewindToSelectionStart);
    const QTextCursor activeCursor = editor->textCursor();
    const int anchorPos = moveCursor ? activeCursor.selectionStart() : (initialCursor.hasSelection() ? initialCursor.selectionStart() : initialCursor.position());
    const Qt::CaseSensitivity sensitivity = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const QString content = editor->toPlainText();
    QList<QTextEdit::ExtraSelection> selections;
    int total = 0;
    int activeOrdinal = 0;
    for (int idx = content.indexOf(text, 0, sensitivity); idx != -1; idx = content.indexOf(text, idx + text.length(), sensitivity)) {
        total++;
        const bool isActive = moveCursor ? (activeCursor.hasSelection() && idx == anchorPos) : (activeOrdinal == 0 && idx + text.length() >= anchorPos);
        activeOrdinal = isActive ? total : activeOrdinal;
        QTextEdit::ExtraSelection selection;
        selection.cursor = activeCursor;
        selection.cursor.setPosition(idx);
        selection.cursor.setPosition(idx + text.length(), QTextCursor::KeepAnchor);
        selection.format.setBackground(QColor(QStringLiteral("#d2992233")));
        selections.append(selection);
    }
    activeOrdinal = (total > 0 && activeOrdinal == 0) ? 1 : activeOrdinal;
    if (activeOrdinal > 0 && activeOrdinal <= selections.size()) {
        selections[activeOrdinal - 1].format.setBackground(QColor(QStringLiteral("#388bfd66")));
    }
    const int initialScroll = editor->verticalScrollBar()->value();
    if (auto* ev = qobject_cast<EditorView*>(editor)) {
        ev->setSearchSelections(selections);
        if (!moveCursor) {
            editor->verticalScrollBar()->setValue(initialScroll);
            if (editor->verticalScrollBar()->value() != initialScroll) {
                editor->verticalScrollBar()->setValue(initialScroll);
            }
        }
        return {activeOrdinal, total};
    }
    editor->setExtraSelections(selections);
    if (!moveCursor) {
        editor->verticalScrollBar()->setValue(initialScroll);
        if (editor->verticalScrollBar()->value() != initialScroll) {
            editor->verticalScrollBar()->setValue(initialScroll);
        }
    }
    return {activeOrdinal, total};
}

bool FindBarOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_findInput || event->type() != QEvent::KeyPress) return QWidget::eventFilter(watched, event);
    auto* keyEvent = static_cast<QKeyEvent*>(event);
    const int key = keyEvent->key();
    const bool isActionKey = (key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter || keyEvent->matches(QKeySequence::FindNext) || keyEvent->matches(QKeySequence::FindPrevious));
    if (!isActionKey) return QWidget::eventFilter(watched, event);
    keyPressEvent(keyEvent);
    return true;
}

void FindBarOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) { emit closeRequested(); event->accept(); return; }
    const bool isEnter = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter);
    const bool isShift = (event->modifiers() & Qt::ShiftModifier) != 0;
    if ((isEnter && isShift) || event->matches(QKeySequence::FindPrevious)) { emit findPreviousRequested(); event->accept(); return; }
    if (isEnter || event->matches(QKeySequence::FindNext)) { emit findNextRequested(); event->accept(); return; }
    QWidget::keyPressEvent(event);
}

void FindBarOverlay::mousePressEvent(QMouseEvent* event) { event->accept(); }
void FindBarOverlay::mouseReleaseEvent(QMouseEvent* event) { event->accept(); }
void FindBarOverlay::mouseDoubleClickEvent(QMouseEvent* event) { event->accept(); }
