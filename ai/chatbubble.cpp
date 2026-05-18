#include "chatbubble.h"

#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>

// ── Markdown → HTML converter (lightweight) ──────────────────────────

static QString escapeHtml(const QString &text)
{
    QString result = text;
    result.replace(QStringLiteral("&"),  QStringLiteral("&amp;"));
    result.replace(QStringLiteral("<"),  QStringLiteral("&lt;"));
    result.replace(QStringLiteral(">"),  QStringLiteral("&gt;"));
    return result;
}

// Process inline formatting: `code`, **bold**, *italic*
static QString processInline(const QString &text)
{
    // Must escape HTML first, then apply inline markup

    // 1. Inline code `code` — protect from other transformations
    QString result;
    static const QRegularExpression codeRe(QStringLiteral("`([^`]+)`"));
    QStringList codePlaceholders;
    int codeIdx = 0;
    {
        int last = 0;
        QRegularExpressionMatchIterator it = codeRe.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            result += escapeHtml(text.mid(last, m.capturedStart() - last));
            QString placeholder = QStringLiteral("\x01CODE%1\x01").arg(codeIdx++);
            codePlaceholders.append(escapeHtml(m.captured(1)));
            result += placeholder;
            last = m.capturedEnd();
        }
        result += escapeHtml(text.mid(last));
    }

    // 2. Bold **text**
    {
        static const QRegularExpression boldRe(QStringLiteral("\\*\\*(.+?)\\*\\*"));
        QString tmp;
        int last = 0;
        QRegularExpressionMatchIterator it = boldRe.globalMatch(result);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            tmp += result.mid(last, m.capturedStart() - last);
            tmp += QStringLiteral("<b>") + m.captured(1) + QStringLiteral("</b>");
            last = m.capturedEnd();
        }
        tmp += result.mid(last);
        result = tmp;
    }

    // 3. Italic *text*
    {
        static const QRegularExpression italicRe(QStringLiteral("\\*(.+?)\\*"));
        QString tmp;
        int last = 0;
        QRegularExpressionMatchIterator it = italicRe.globalMatch(result);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            tmp += result.mid(last, m.capturedStart() - last);
            tmp += QStringLiteral("<i>") + m.captured(1) + QStringLiteral("</i>");
            last = m.capturedEnd();
        }
        tmp += result.mid(last);
        result = tmp;
    }

    // 4. Links [text](url)
    {
        static const QRegularExpression linkRe(QStringLiteral("\\[([^\\]]+)\\]\\(([^)]+)\\)"));
        QString tmp;
        int last = 0;
        QRegularExpressionMatchIterator it = linkRe.globalMatch(result);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            tmp += result.mid(last, m.capturedStart() - last);
            tmp += QStringLiteral("<a href=\"") + escapeHtml(m.captured(2))
                 + QStringLiteral("\" style=\"color:#4da3ff;\">")
                 + escapeHtml(m.captured(1)) + QStringLiteral("</a>");
            last = m.capturedEnd();
        }
        tmp += result.mid(last);
        result = tmp;
    }

    // 5. Restore code placeholders
    for (int i = 0; i < codePlaceholders.size(); ++i) {
        result.replace(QStringLiteral("\x01CODE%1\x01").arg(i),
                       QStringLiteral("<code style=\"background-color:#3c3c3c; color:#ce9178; "
                                      "padding:1px 4px; border-radius:3px; font-size:12px;\">")
                       + codePlaceholders[i]
                       + QStringLiteral("</code>"));
    }

    return result;
}

QString ChatBubble::markdownToHtml(const QString &md)
{
    if (md.isEmpty())
        return QStringLiteral("<p></p>");

    QString html;
    QStringList lines = md.split(QStringLiteral("\n"));

    bool inCodeBlock = false;
    QString codeBlockContent;
    QString codeBlockLang;

    for (const QString &rawLine : lines) {
        QString line = rawLine; // keep original for code block detection

        if (line.startsWith(QStringLiteral("```"))) {
            if (inCodeBlock) {
                // Close code block
                html += QStringLiteral("<pre style=\"background-color:#1e1e1e; color:#d4d4d4; "
                                       "padding:8px; border-radius:4px; font-size:12px; "
                                       "overflow-x:auto;\"><code>")
                      + escapeHtml(codeBlockContent)
                      + QStringLiteral("</code></pre>\n");
                codeBlockContent.clear();
                inCodeBlock = false;
            } else {
                inCodeBlock = true;
                codeBlockLang = line.mid(3).trimmed();
                codeBlockContent.clear();
            }
            continue;
        }

        if (inCodeBlock) {
            if (!codeBlockContent.isEmpty())
                codeBlockContent += QStringLiteral("\n");
            codeBlockContent += line;
            continue;
        }

        // Skip empty lines in paragraph context — they just add newlines
        // We treat them as paragraph breaks
        if (line.trimmed().isEmpty()) {
            // End current paragraph — we'll handle this implicitly
            // by adding a line break in the paragraph flow
            continue;
        }

        // Headings ## text
        static const QRegularExpression hRe(QStringLiteral("^(#{1,6})\\s+(.+)$"));
        QRegularExpressionMatch hMatch = hRe.match(line);
        if (hMatch.hasMatch()) {
            int level = hMatch.captured(1).length();
            QString headingText = processInline(hMatch.captured(2));
            html += QStringLiteral("<h%1 style=\"color:#ffffff; margin:8px 0 4px 0;\">%2</h%1>\n")
                        .arg(level).arg(headingText);
            continue;
        }

        // Unordered list - * item or - item
        static const QRegularExpression ulRe(QStringLiteral("^[\\*\\-]\\s+(.+)$"));
        QRegularExpressionMatch ulMatch = ulRe.match(line);
        if (ulMatch.hasMatch()) {
            html += QStringLiteral("<li style=\"color:#d4d4d4; margin:2px 0;\">%1</li>\n")
                        .arg(processInline(ulMatch.captured(1)));
            continue;
        }

        // Numbered list 1. item
        static const QRegularExpression olRe(QStringLiteral("^\\d+\\.\\s+(.+)$"));
        QRegularExpressionMatch olMatch = olRe.match(line);
        if (olMatch.hasMatch()) {
            html += QStringLiteral("<li style=\"color:#d4d4d4; margin:2px 0;\">%1</li>\n")
                        .arg(processInline(olMatch.captured(1)));
            continue;
        }

        // Horizontal rule ---
        static const QRegularExpression hrRe(QStringLiteral("^\\-{3,}\\s*$"));
        if (hrRe.match(line).hasMatch()) {
            html += QStringLiteral("<hr style=\"border:0; border-top:1px solid #555; margin:8px 0;\">\n");
            continue;
        }

        // Regular paragraph line — wrap in <p> if not continuation
        html += QStringLiteral("<p style=\"color:#d4d4d4; margin:4px 0;\">%1</p>\n")
                    .arg(processInline(line));
    }

    // Close unclosed code block
    if (inCodeBlock) {
        html += QStringLiteral("<pre style=\"background-color:#1e1e1e; color:#d4d4d4; "
                               "padding:8px; border-radius:4px; font-size:12px; "
                               "overflow-x:auto;\"><code>")
              + escapeHtml(codeBlockContent)
              + QStringLiteral("</code></pre>\n");
    }

    return html;
}

// ── ChatBubble ─────────────────────────────────────────────────────

ChatBubble::ChatBubble(Role role, const QString &text, QWidget *parent)
    : QWidget(parent)
    , m_role(role)
    , m_text(text)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(2);

    // Role label
    m_roleLabel = new QLabel(role == User ? tr("你") : tr("AI 助手"));
    m_roleLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; font-weight: bold; padding: 0 4px;")
            .arg(role == User ? "#4da3ff" : "#6a9955")
    );

    // Content browser (QTextBrowser for HTML rendering)
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->setStyleSheet(messageStyleSheet());
    m_browser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    // Make QTextBrowser auto-resize height to fit content.
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
    bubbleInner->setStyleSheet(
        QStringLiteral(
            "#bubbleInner {"
            "  background-color: %1;"
            "  border-radius: 8px;"
            "  padding: 6px 10px;"
            "}"
        ).arg(role == User ? "#1a3a5c" : "#2d2d2d")
    );
    auto *innerLayout = new QVBoxLayout(bubbleInner);
    innerLayout->setContentsMargins(8, 6, 8, 6);
    innerLayout->setSpacing(0);
    innerLayout->addWidget(m_browser);

    if (role == User) {
        bubbleRow->addStretch();
        bubbleRow->addWidget(bubbleInner);
        outerLayout->addWidget(m_roleLabel, 0, Qt::AlignRight);
    } else {
        bubbleRow->addWidget(bubbleInner);
        bubbleRow->addStretch();
        outerLayout->addWidget(m_roleLabel);
    }

    outerLayout->addLayout(bubbleRow);

    // Set initial content
    updateContent();

    // Set a max width proportionally to parent
    if (parent) {
        int pw = parent->width();
        if (pw > 0)
            m_browser->setMaximumWidth(qMax(200, pw * 7 / 10));
    }
}

void ChatBubble::setText(const QString &text)
{
    m_text = text;
    updateContent();
}

void ChatBubble::appendText(const QString &text)
{
    m_text += text;
    updateContent();
}

void ChatBubble::updateContent()
{
    if (m_role == Assistant) {
        QString html = markdownToHtml(m_text);
        // Wrap in a div with dark theme base
        html = QStringLiteral(
            "<div style=\"color:#d4d4d4; font-size:13px; line-height:1.5;\">%1</div>"
        ).arg(html);
        m_browser->setHtml(html);
    } else {
        // User: plain text in a label-like format
        QString escaped = m_text;
        escaped.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
        escaped.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
        escaped.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
        escaped.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
        m_browser->setHtml(
            QStringLiteral(
                "<div style=\"color:#d4d4d4; font-size:13px; line-height:1.5;\">%1</div>"
            ).arg(escaped)
        );
    }

    // Force height update after setHtml() completes, so the layout is final.
    // Do NOT rely on contentsChanged — it fires during setHtml() processing
    // when document layout may be incomplete or transient.
    QTextDocument *doc = m_browser->document();
    doc->adjustSize();
    // Ensure layout is up-to-date at the current viewport width
    doc->setTextWidth(m_browser->viewport()->width());
    int docHeight = qCeil(doc->size().height());
    m_browser->setFixedHeight(docHeight + 4);
}

QString ChatBubble::messageStyleSheet() const
{
    return QStringLiteral(
        "QTextBrowser {"
        "  background: transparent;"
        "  color: #d4d4d4;"
        "  font-size: 13px;"
        "  selection-background-color: #264F78;"
        "}"
        "QTextBrowser a {"
        "  color: #4da3ff;"
        "}"
    );
}
