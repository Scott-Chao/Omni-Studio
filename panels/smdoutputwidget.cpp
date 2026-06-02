#include "smdoutputwidget.h"
#include "core/thememanager.h"
#include <QFont>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QRegularExpression>

enum class CompilerSeverity { None, Warning, Error };

static CompilerSeverity detectSeverity(const QString &line)
{
    // GCC/Clang:  file:line:col: warning|error|note:
    static const QRegularExpression gccRe(
        QStringLiteral("^(.+?):(\\d+):(\\d+):\\s*(warning|error|note):\\s*"));
    QRegularExpressionMatch m = gccRe.match(line.trimmed());
    if (m.hasMatch()) {
        QString s = m.captured(4).toLower();
        if (s == QStringLiteral("warning")) return CompilerSeverity::Warning;
        if (s == QStringLiteral("error"))   return CompilerSeverity::Error;
        return CompilerSeverity::None;
    }
    // MSVC: file(line,col): warning|error Cxxxx:
    static const QRegularExpression msvcRe(
        QStringLiteral("^(.+?)\\((\\d+)(?:,(\\d+))?\\)\\s*:\\s*(warning|error)\\s+(C\\d+):"));
    m = msvcRe.match(line.trimmed());
    if (m.hasMatch()) {
        QString s = m.captured(4).toLower();
        if (s == QStringLiteral("warning")) return CompilerSeverity::Warning;
        if (s == QStringLiteral("error"))   return CompilerSeverity::Error;
    }
    return CompilerSeverity::None;
}

// GCC preamble lines that introduce the function being compiled:
//   "file: In function '...':" / "file: In member function '...':" / etc.
static bool isGccPreamble(const QString &line)
{
    return line.contains(QStringLiteral(": In function "))
        || line.contains(QStringLiteral(": In member function "))
        || line.contains(QStringLiteral(": In constructor "))
        || line.contains(QStringLiteral(": In destructor "));
}

SmdOutputWidget::SmdOutputWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_outputEdit = new QPlainTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setFocusPolicy(Qt::StrongFocus);
    m_outputEdit->setMaximumBlockCount(0);
    m_outputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_outputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QFont font(QStringLiteral("Consolas"), 10);
    font.setStyleHint(QFont::Monospace);
    m_outputEdit->setFont(font);

    layout->addWidget(m_outputEdit);

    connect(m_outputEdit->document(), &QTextDocument::blockCountChanged,
            this, &SmdOutputWidget::updateHeight);

    ThemeManager::watchTheme(this, &SmdOutputWidget::refreshStyle);
    refreshStyle();

    setVisible(false);
}

void SmdOutputWidget::refreshStyle()
{
    auto &tm = ThemeManager::instance();
    m_outputEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background-color: %1; color: %2; selection-background-color: %3; border: none; "
        "border-top: 1px solid %4; padding: 2px 8px; }"
    ).arg(tm.color("output.background").name(),
          tm.color("output.foreground").name(),
          tm.color("output.selectionBackground").name(),
          tm.color("panel.border").name()));
}

void SmdOutputWidget::setOutput(const QString &text)
{
    m_hiddenLineCount = 0;

    // Block signals during bulk text set to avoid premature updateHeight()
    // before the widget is visible and properly laid out.
    m_outputEdit->document()->blockSignals(true);

    if (text.startsWith(QStringLiteral("<!DOCTYPE HTML"), Qt::CaseInsensitive)) {
        // HTML format (saved by current code) — restore with colors
        m_outputEdit->document()->setHtml(text);
    } else {
        // Plain text format (legacy files or direct calls)
        QStringList lines = text.split(QLatin1Char('\n'));
        if (!lines.isEmpty() && lines.constLast().isEmpty())
            lines.removeLast();
        if (lines.size() > kMaxOutputLines) {
            m_hiddenLineCount = lines.size() - kMaxOutputLines;
            lines = lines.mid(0, kMaxOutputLines);
            lines.append(QStringLiteral("[%1 more lines]").arg(m_hiddenLineCount));
        }
        m_outputEdit->setPlainText(lines.join(QLatin1Char('\n')));
    }

    m_outputEdit->document()->blockSignals(false);

    setVisible(true);
    // Defer height update to ensure font metrics and layout are valid
    QTimer::singleShot(0, this, &SmdOutputWidget::updateHeight);
}

void SmdOutputWidget::appendText(const QString &text, bool isStderr)
{
    setVisible(true);
    QTextCursor cursor = m_outputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    // Remove existing indicator so it doesn't skew the block count
    if (m_hiddenLineCount > 0) {
        cursor.movePosition(QTextCursor::End);
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        if (cursor.position() > 0)
            cursor.deletePreviousChar();
    }

    // Append the new text
    if (isStderr) {
        auto &tm = ThemeManager::instance();
        const QColor warningColor = tm.color("diagnostics.warning");
        const QColor errorColor = tm.color("output.stderr");
        const bool trailingNewline = text.endsWith(QLatin1Char('\n'));
        const QStringList lines = text.split(QLatin1Char('\n'));
        bool lastWasWarning = false;
        for (int i = 0; i < lines.size(); ++i) {
            if (i == lines.size() - 1 && lines[i].isEmpty() && trailingNewline)
                continue; // skip trailing empty string from split

            QTextCharFormat fmt;
            CompilerSeverity sev = detectSeverity(lines[i]);

            if (sev == CompilerSeverity::Warning) {
                fmt.setForeground(warningColor);
                lastWasWarning = true;
            } else if (sev == CompilerSeverity::Error) {
                fmt.setForeground(errorColor);
                lastWasWarning = false;
            } else if (lastWasWarning && !lines[i].isEmpty()
                       && lines[i].at(0) == QLatin1Char(' ')) {
                // Indented continuation (code context / caret) of a warning
                fmt.setForeground(warningColor);
            } else if (isGccPreamble(lines[i])) {
                // Look ahead to see what severity follows this preamble
                bool followedByWarning = false;
                for (int j = i + 1; j < lines.size(); ++j) {
                    CompilerSeverity nextSev = detectSeverity(lines[j]);
                    if (nextSev == CompilerSeverity::Warning) { followedByWarning = true; break; }
                    if (nextSev == CompilerSeverity::Error) break;
                }
                fmt.setForeground(followedByWarning ? warningColor : errorColor);
                lastWasWarning = followedByWarning;
            } else {
                fmt.setForeground(errorColor);
                lastWasWarning = false;
            }

            cursor.insertText(lines[i], fmt);
            // Restore newline between lines and after final if input ended with \n
            if (i < lines.size() - 1 || trailingNewline)
                cursor.insertText(QStringLiteral("\n"), fmt);
        }
    } else {
        cursor.insertText(text, QTextCharFormat());
    }

    // Enforce max output lines: keep first kMaxOutputLines, drop from the end.
    // Don't count trailing empty block (artifact of \n-terminated output).
    int blockCount = m_outputEdit->document()->blockCount();
    if (blockCount > 1 && m_outputEdit->document()->lastBlock().text().isEmpty())
        --blockCount;
    if (blockCount > kMaxOutputLines) {
        int newHidden = blockCount - kMaxOutputLines;
        m_hiddenLineCount += newHidden;

        QTextBlock targetBlock = m_outputEdit->document()->findBlockByNumber(kMaxOutputLines);
        cursor.movePosition(QTextCursor::End);
        cursor.setPosition(targetBlock.position(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();

        cursor.movePosition(QTextCursor::End);
        QTextCharFormat grayFmt;
        grayFmt.setForeground(ThemeManager::instance().color("editorLineNumber.foreground"));
        cursor.insertText(QStringLiteral("\n[%1 more lines]").arg(m_hiddenLineCount), grayFmt);
    } else if (m_hiddenLineCount > 0) {
        // Re-add indicator after removing it, since we're still under the limit
        cursor.movePosition(QTextCursor::End);
        QTextCharFormat grayFmt;
        grayFmt.setForeground(ThemeManager::instance().color("editorLineNumber.foreground"));
        cursor.insertText(QStringLiteral("\n[%1 more lines]").arg(m_hiddenLineCount), grayFmt);
    }
}

void SmdOutputWidget::clearOutput()
{
    m_hiddenLineCount = 0;
    m_outputEdit->clear();
    setVisible(false);
}

void SmdOutputWidget::clearSelection()
{
    QTextCursor c = m_outputEdit->textCursor();
    if (c.hasSelection()) {
        c.clearSelection();
        m_outputEdit->setTextCursor(c);
    }
}

void SmdOutputWidget::scrollToTop()
{
    QTextCursor cursor = m_outputEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_outputEdit->setTextCursor(cursor);
}

QString SmdOutputWidget::outputText() const
{
    if (m_outputEdit->toPlainText().isEmpty())
        return {};
    return m_outputEdit->document()->toHtml();
}

bool SmdOutputWidget::hasOutput() const
{
    return !m_outputEdit->toPlainText().isEmpty();
}

void SmdOutputWidget::updateHeight()
{
    if (!m_outputEdit)
        return;

    int blockCount = m_outputEdit->document()->blockCount();
    int lineCount = qBound(1, blockCount, kMaxVisibleLines);

    // Disable scrollbar when content fits within max visible lines
    if (blockCount <= kMaxVisibleLines)
        m_outputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    else
        m_outputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QFontMetrics fm(m_outputEdit->font());
    int contentH = lineCount * fm.lineSpacing();
    int docMargin = static_cast<int>(m_outputEdit->document()->documentMargin()) * 2;
    QMargins cm = m_outputEdit->contentsMargins();
    int frameW = static_cast<int>(m_outputEdit->frameWidth()) * 2;
    int totalH = contentH + docMargin + cm.top() + cm.bottom() + frameW;

    m_outputEdit->setFixedHeight(totalH);
    setFixedHeight(totalH);
}
