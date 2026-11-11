#include "ui/InstantPreviewSurface.h"
#include "parser/TocParser.h"
#include <QAbstractTextDocumentLayout>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFont>
#include <QMap>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <algorithm>
#include <cmath>
namespace {
QStringList platformProseFamilies() {
#if defined(Q_OS_MACOS)
    return {QStringLiteral("Helvetica Neue"), QStringLiteral("Apple Symbols")};
#elif defined(Q_OS_WIN)
    return {QStringLiteral("Segoe UI")};
#else
    return {};
#endif
}
QString platformMonoFamily() {
#if defined(Q_OS_MACOS)
    return QStringLiteral("Menlo");
#elif defined(Q_OS_WIN)
    return QStringLiteral("Consolas");
#else
    return QStringLiteral("Monospace");
#endif
}
bool isCodeFormat(const QTextCharFormat& fmt) {
    if (fmt.fontFixedPitch() || fmt.intProperty(QTextFormat::FontStyleHint) == QFont::Monospace) return true;
    return fmt.hasProperty(QTextFormat::FontFamilies) || fmt.hasProperty(QTextFormat::FontFamily);
}
QTextCharFormat normalizedCodeFormat(const QTextCharFormat& baseFmt) {
    QTextCharFormat codeFmt = baseFmt;
    codeFmt.clearProperty(QTextFormat::FontFamily);
    codeFmt.clearProperty(QTextFormat::FontStyleHint);
    codeFmt.setFontFamilies({platformMonoFamily()});
    codeFmt.setFontFixedPitch(true);
    codeFmt.setBackground(QColor("#161b22"));
    codeFmt.setForeground(QColor("#e6edf3"));
    return codeFmt;
}
void styleBlockCodeFragments(QTextCursor& editCursor, const QTextBlock& block) {
    if (isCodeFormat(block.charFormat())) {
        editCursor.setPosition(block.position());
        editCursor.setBlockCharFormat(normalizedCodeFormat(block.charFormat()));
    }
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment frag = it.fragment();
        if (!frag.isValid() || !isCodeFormat(frag.charFormat())) continue;
        editCursor.setPosition(frag.position());
        editCursor.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
        editCursor.setCharFormat(normalizedCodeFormat(frag.charFormat()));
    }
}
void recordHeadingPosition(const QTextBlock& block, QMap<QString, int>& seenSlugs, QHash<QString, int>& headingPositions) {
    if (block.blockFormat().headingLevel() <= 0) return;
    headingPositions.insert(TocParser::slugify(TocParser::cleanHeadingText(block.text()), seenSlugs), block.position());
}
void indexAndStyleDocument(QTextDocument* doc, QHash<QString, int>& headingPositions) {
    QMap<QString, int> seenSlugs;
    QTextCursor editCursor(doc);
    editCursor.beginEditBlock();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        recordHeadingPosition(block, seenSlugs, headingPositions);
        styleBlockCodeFragments(editCursor, block);
    }
    editCursor.endEditBlock();
}
QString resolveDocumentDirectoryUrl(const QString& baseFilePath) {
    if (baseFilePath.trimmed().isEmpty()) return QString();
    const QString dirPath = QFileInfo(baseFilePath).absolutePath();
    return dirPath.isEmpty() ? QString() : QUrl::fromLocalFile(dirPath + QLatin1Char('/')).toString();
}
QString maskDeferredSyntax(QString md) {
    if (md.contains(QLatin1String("```mermaid"))) {
        static const QRegularExpression mermaidRe(QStringLiteral("(?ms)^[ \\t]*```mermaid[^\\n]*\\n.*?^[ \\t]*```[ \\t]*$"));
        md.replace(mermaidRe, QStringLiteral("> *Diagram*"));
    }
    if (md.contains(QLatin1Char('$'))) {
        static const QRegularExpression blockMathRe(QStringLiteral("(?ms)\\$\\$.+?\\$\\$"));
        static const QRegularExpression inlineMathRe(QStringLiteral("\\$(?!\\s)([^$\\n]*[\\\\^_][^$\\n]*)(?<!\\s)\\$"));
        md.replace(blockMathRe, QStringLiteral("> *Equation*"));
        md.replace(inlineMathRe, QStringLiteral("…"));
    }
    return md;
}
} // namespace

InstantPreviewSurface::InstantPreviewSurface(QWidget* parent)
    : QTextBrowser(parent)
    , m_externalUrlHandler([](const QUrl& url) { QDesktopServices::openUrl(url); }) {
    setObjectName(QStringLiteral("instantPreviewSurface"));
    setReadOnly(true);
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setAttribute(Qt::WA_InputMethodEnabled, false);
    setFrameShape(QFrame::NoFrame);
    configureObsidianPalette();
    document()->setDocumentMargin(0.0);
    applyZoomFactor(1.0);
    connect(this, &QTextBrowser::anchorClicked, this, &InstantPreviewSurface::handleAnchorActivated);
}
void InstantPreviewSurface::resizeEvent(QResizeEvent* event) {
    QTextBrowser::resizeEvent(event);
    syncEditorialMargins();
}
void InstantPreviewSurface::syncEditorialMargins() {
    const int px = font().pixelSize() > 0 ? font().pixelSize() : 15;
    const qreal zoom = std::max(0.25, static_cast<qreal>(px) / 15.0);
    const qreal side = (width() < 160) ? 16.0 : (std::max(0.0, (width() - 760.0 * zoom) * 0.5) + 48.0 * zoom);
    QTextFrame* root = document()->rootFrame();
    QTextFrameFormat fmt = root->frameFormat();
    fmt.setTopMargin(40.0 * zoom);
    fmt.setBottomMargin(64.0 * zoom);
    fmt.setLeftMargin(side);
    fmt.setRightMargin(side);
    root->setFrameFormat(fmt);
}
void InstantPreviewSurface::configureObsidianPalette() {
    QPalette pal = palette();
    for (auto role : {QPalette::Base, QPalette::Window}) pal.setColor(role, QColor("#050505"));
    for (auto role : {QPalette::Text, QPalette::WindowText}) pal.setColor(role, QColor("#f0f6fc"));
    pal.setColor(QPalette::Link, QColor("#58a6ff"));
    setPalette(pal); setAutoFillBackground(true);
    viewport()->setPalette(pal); viewport()->setAutoFillBackground(true);
    setStyleSheet(QStringLiteral("QTextBrowser { background-color: #050505; color: #f0f6fc; border: none; }"));
}
void InstantPreviewSurface::renderMarkdown(const QString& markdown, const QString& baseFilePath) {
    m_headingPositions.clear();
    document()->setMetaInformation(QTextDocument::DocumentUrl, resolveDocumentDirectoryUrl(baseFilePath));
    if (markdown.isEmpty()) { clear(); syncEditorialMargins(); return; }
    document()->setMarkdown(maskDeferredSyntax(markdown), QTextDocument::MarkdownDialectGitHub);
    indexAndStyleDocument(document(), m_headingPositions);
    syncEditorialMargins();
}
double InstantPreviewSurface::verticalScrollRatio() const {
    QScrollBar* bar = verticalScrollBar();
    return (!bar || bar->maximum() <= 0) ? 0.0 : std::clamp(static_cast<double>(bar->value()) / static_cast<double>(bar->maximum()), 0.0, 1.0);
}
void InstantPreviewSurface::setVerticalScrollRatio(double ratio) {
    QScrollBar* bar = verticalScrollBar();
    if (bar && bar->maximum() > 0) bar->setValue(static_cast<int>(std::llround(std::clamp(ratio, 0.0, 1.0) * static_cast<double>(bar->maximum()))));
}
void InstantPreviewSurface::scrollToAnchorName(const QString& anchor) {
    const QString key = anchor.trimmed();
    if (key.isEmpty() || !m_headingPositions.contains(key)) return;
    const QTextBlock block = document()->findBlock(m_headingPositions.value(key));
    if (block.isValid() && verticalScrollBar()) {
        verticalScrollBar()->setValue(static_cast<int>(std::llround(document()->documentLayout()->blockBoundingRect(block).top())));
    }
}
void InstantPreviewSurface::applyZoomFactor(qreal factor) {
    QFont current = font();
    const QStringList prose = platformProseFamilies();
    if (!prose.isEmpty()) current.setFamilies(prose);
    current.setPixelSize(std::max(4, static_cast<int>(std::lround(15.0 * std::clamp(factor, 0.25, 3.0)))));
    setFont(current);
    document()->setDefaultFont(current);
    syncEditorialMargins();
}
bool InstantPreviewSurface::findString(const QString& subString, bool backward, bool caseSensitive) {
    if (subString.isEmpty()) {
        QTextCursor cursor = textCursor();
        cursor.clearSelection(); setTextCursor(cursor);
        return false;
    }
    QTextDocument::FindFlags flags;
    if (backward) flags |= QTextDocument::FindBackward;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    if (find(subString, flags)) return true;
    QTextCursor wrapCursor = textCursor();
    wrapCursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
    setTextCursor(wrapCursor);
    return find(subString, flags);
}
int InstantPreviewSurface::countOccurrences(const QString& subString, bool caseSensitive) const { return subString.isEmpty() ? 0 : toPlainText().count(subString, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive); }
void InstantPreviewSurface::copySelection() { copy(); }
void InstantPreviewSurface::selectAllContent() { selectAll(); }
void InstantPreviewSurface::setExternalUrlHandler(std::function<void(const QUrl&)> handler) { if (handler) m_externalUrlHandler = std::move(handler); }
void InstantPreviewSurface::handleAnchorActivated(const QUrl& url) {
    if (!url.isValid()) return;
    if (url.scheme().isEmpty() && url.path().isEmpty() && !url.fragment().isEmpty()) {
        scrollToAnchorName(url.fragment());
        emit internalAnchorActivated(url.fragment());
        return;
    }
    const QString s = url.scheme().toLower();
    if (s != QLatin1String("http") && s != QLatin1String("https") && s != QLatin1String("mailto")) return;
    if (m_externalUrlHandler) m_externalUrlHandler(url);
    emit externalLinkActivated(url);
}
