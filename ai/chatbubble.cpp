#include "chatbubble.h"
#include "core/thememanager.h"
#include "core/utilities.h"

#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QTimer>
#include <QScrollBar>

// processInline moved to MarkdownUtils in core/utilities.h

QString ChatBubble::markdownToHtml(const QString &md, const QColor &textColor,
                                    const QColor &codeBg, const QColor &codeFg,
                                    const QColor &linkColor, const QColor &selectionBg,
                                    const QColor &headingColor)
{
    return MarkdownUtils::markdownToHtml(md, textColor, codeBg, codeFg,
                                         linkColor, selectionBg, headingColor);
}

// ── Incremental streaming helpers ───────────────────────────

bool ChatBubble::isStructuralDelta(const QString &delta) const
{
    // Code block fences — the only multi-line structural marker
    if (delta.contains(QStringLiteral("```")))
        return true;

    // Inside a code block: every delta must go through full markdownToHtml
    // for correct <pre><code> rendering (simple <p> wrapping would be wrong).
    if (m_inCodeBlock)
        return true;

    // Per-line structural markers: headings, lists, horizontal rules, blockquotes
    QStringList lines = delta.split(QStringLiteral("\n"));
    static const QRegularExpression structuralRe(
        QStringLiteral("^\\s*(#{1,6}\\s+|[*\\-]\\s+|\\d+\\.\\s+|-{3,}\\s*$|>\\s*)")
    );
    for (const QString &line : lines) {
        if (structuralRe.match(line).hasMatch())
            return true;
    }
    return false;
}

QString ChatBubble::processSimpleDelta(const QString &delta,
                                        const QColor &textColor,
                                        const QColor &codeBg,
                                        const QColor &codeFg,
                                        const QColor &linkColor) const
{
    if (delta.isEmpty())
        return {};

    QString html;
    QStringList lines = delta.split(QStringLiteral("\n"));
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty())
            continue;
        html += QStringLiteral("<p style=\"color:%1; margin:4px 0;\">%2</p>\n")
                    .arg(textColor.name(),
                         MarkdownUtils::processInline(line, codeBg, codeFg, linkColor));
    }
    return html;
}

// ── ChatBubble ─────────────────────────────────────────────────────

ChatBubble::ChatBubble(MessageRole role, const QString &text, QWidget *parent)
    : QWidget(parent)
    , m_role(role)
    , m_text(text)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(2);

    // Role label
    m_roleLabel = new QLabel(role == MessageRole::User ? tr("你") : tr("AI 助手"));

    // Content browser (QTextBrowser for HTML rendering)
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_browser->document()->setDocumentMargin(0);

    // We update height synchronously after setHtml(), NOT via contentsChanged,
    // because contentsChanged fires mid-setHtml when document layout is incomplete.
    m_browser->setDocument(m_browser->document());

    // Layout: role label then bubble
    auto *bubbleRow = new QHBoxLayout;
    bubbleRow->setContentsMargins(4, 0, 4, 0);
    bubbleRow->setSpacing(0);

    // Container for the actual bubble with background
    auto *bubbleInner = new QWidget;
    bubbleInner->setObjectName(QStringLiteral("bubbleInner"));
    auto *innerLayout = new QVBoxLayout(bubbleInner);
    innerLayout->setContentsMargins(8, 6, 8, 6);
    innerLayout->setSpacing(0);
    innerLayout->addWidget(m_browser);

    if (role == MessageRole::User) {
        bubbleRow->addStretch(1);
        bubbleRow->addWidget(bubbleInner, 9);
        outerLayout->addWidget(m_roleLabel, 0, Qt::AlignRight);
    } else {
        bubbleRow->addWidget(bubbleInner, 9);
        bubbleRow->addStretch(1);
        outerLayout->addWidget(m_roleLabel);
    }

    outerLayout->addLayout(bubbleRow);

    // Debounce timer for streaming updates — avoid O(n²) re-renders
    // on every SSE chunk. Timer fires at most every 80ms, coalescing
    // rapid chunks into a single updateContent() call.
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(UPDATE_INTERVAL_MS);
    connect(m_updateTimer, &QTimer::timeout, this, &ChatBubble::updateContent);

    ThemeManager::watchTheme(this, &ChatBubble::refreshStyle);
    refreshStyle();
}

void ChatBubble::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    QColor roleColor = tm.color(m_role == MessageRole::User ? "aiAssistant.roleUser" : "aiAssistant.roleAssistant");
    QColor bubbleBg = tm.color(m_role == MessageRole::User ? "aiAssistant.bubbleUser" : "aiAssistant.bubbleAssistant");

    m_roleLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px; font-weight: bold; padding: 0 4px;"
    ).arg(roleColor.name()));

    // Use cascading QSS with object name to style the inner bubble container
    setStyleSheet(QStringLiteral(
        "#bubbleInner {"
        "  background-color: %1;"
        "  border-radius: 8px;"
        "  padding: 6px 10px;"
        "}"
    ).arg(bubbleBg.name()));

    m_browser->setStyleSheet(messageStyleSheet());

    m_fullRebuildNeeded = true;
    updateContent();
}

void ChatBubble::setText(const QString &text)
{
    m_text = text;
    m_accumulatedHtml.clear();
    m_lastProcessedLength = 0;
    m_fullRebuildNeeded = false;
    m_inCodeBlock = false;
    updateContent();
}

void ChatBubble::appendText(const QString &text)
{
    m_text += text;
    // Debounce: restart timer instead of immediately re-rendering.
    // Multiple rapid chunks coalesce into a single updateContent() call.
    m_updateTimer->start();
}

void ChatBubble::flushUpdate()
{
    if (m_updateTimer->isActive()) {
        m_updateTimer->stop();
        updateContent();
    }
    // Final full rebuild to correct any incremental approximation
    // (e.g. list-item text split across chunks that was treated as <p>).
    m_fullRebuildNeeded = true;
    updateContent();
}

void ChatBubble::updateContent()
{
    auto &tm = ThemeManager::instance();
    QColor textColor = tm.color("aiAssistant.foreground");
    QColor codeBg = tm.color("aiAssistant.codeBackground");
    QColor codeFg = tm.color("aiAssistant.codeForeground");
    QColor linkColor = tm.color("aiAssistant.linkColor");
    QColor selectionBg = tm.color("aiAssistant.selectionBackground");
    QColor headingColor = textColor;

    // Suppress painting during setHtml to avoid the blank flash that
    // occurs when QTextDocument clears its content before re-parsing.
    m_browser->setUpdatesEnabled(false);

    if (m_role == MessageRole::Assistant) {
        if (m_fullRebuildNeeded || m_accumulatedHtml.isEmpty()) {
            // Full rebuild: theme change, first-time, or final flush
            m_accumulatedHtml = markdownToHtml(m_text, textColor, codeBg, codeFg,
                                               linkColor, selectionBg, headingColor);
            m_lastProcessedLength = m_text.length();
            m_fullRebuildNeeded = false;

            // Recalculate code block state from full text so subsequent
            // incremental deltas inside a code block are correctly detected
            // as structural.
            m_inCodeBlock = false;
            int fenceIdx = 0;
            while ((fenceIdx = m_text.indexOf(QStringLiteral("```"), fenceIdx)) != -1) {
                m_inCodeBlock = !m_inCodeBlock;
                fenceIdx += 3;
            }
        } else {
            // Incremental: only process new text since last conversion
            QString delta = m_text.mid(m_lastProcessedLength);
            if (!delta.isEmpty()) {
                // Toggle code block state by counting fences only in the delta,
                // avoiding an O(n) re-scan of the full accumulated text each tick.
                int fenceCount = 0;
                int fenceIdx = 0;
                while ((fenceIdx = delta.indexOf(QStringLiteral("```"), fenceIdx)) != -1) {
                    ++fenceCount;
                    fenceIdx += 3;
                }
                if (fenceCount % 2 == 1)
                    m_inCodeBlock = !m_inCodeBlock;

                if (isStructuralDelta(delta)) {
                    // Structural boundary → full rebuild for correctness
                    m_accumulatedHtml = markdownToHtml(m_text, textColor, codeBg, codeFg,
                                                       linkColor, selectionBg, headingColor);
                } else {
                    // Simple text continuation → inline-only conversion
                    m_accumulatedHtml += processSimpleDelta(delta, textColor, codeBg, codeFg, linkColor);
                }
                m_lastProcessedLength = m_text.length();
            }
        }

        QString html = QStringLiteral(
            "<div style=\"color:%1; font-size:13px; line-height:1.5;\">%2</div>"
        ).arg(textColor.name(), m_accumulatedHtml);
        m_browser->setHtml(html);
    } else {
        QString escaped = m_text.toHtmlEscaped();
        escaped.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
        m_browser->setHtml(
            QStringLiteral(
                "<div style=\"color:%1; font-size:13px; line-height:1.5;\">%2</div>"
            ).arg(textColor.name(), escaped)
        );
    }

    updateBrowserHeight();

    m_browser->setUpdatesEnabled(true);
    m_browser->viewport()->update();
}

void ChatBubble::updateBrowserHeight()
{
    QTextDocument *doc = m_browser->document();

    int w = m_browser->viewport()->width();
    if (w <= 0)
        w = 200;

    doc->setTextWidth(w);

    int docHeight = qCeil(doc->size().height());
    // Only update if height actually changed to avoid infinite resize loops.
    if (qAbs(docHeight - m_browser->height()) > 1)
        m_browser->setFixedHeight(docHeight);
}

void ChatBubble::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Recalculate browser height when bubble gets its final layout width.
    // The constructor's updateContent() might run before the widget has its
    // actual width, leading to a stale fixed height and blank space at bottom.
    updateBrowserHeight();
}

QString ChatBubble::messageStyleSheet() const
{
    auto &tm = ThemeManager::instance();
    return QStringLiteral(
        "QTextBrowser {"
        "  background: transparent;"
        "  color: %1;"
        "  font-size: 13px;"
        "  selection-background-color: %2;"
        "}"
        "QTextBrowser a {"
        "  color: %3;"
        "}"
    ).arg(tm.color("aiAssistant.foreground").name(),
          tm.color("aiAssistant.selectionBackground").name(),
          tm.color("aiAssistant.linkColor").name());
}
