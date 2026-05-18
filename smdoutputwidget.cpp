#include "smdoutputwidget.h"
#include <QFont>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>

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
    m_outputEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background-color: #1E1E1E; color: #D4D4D4; "
        "selection-background-color: #264F78; border: none; "
        "border-top: 1px solid #3c3c3c; padding: 2px 8px; }"
    ));
    QFont font(QStringLiteral("Consolas"), 10);
    font.setStyleHint(QFont::Monospace);
    m_outputEdit->setFont(font);

    layout->addWidget(m_outputEdit);

    connect(m_outputEdit->document(), &QTextDocument::blockCountChanged,
            this, &SmdOutputWidget::updateHeight);

    setVisible(false);
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
        QTextCharFormat fmt;
        fmt.setForeground(QColor(QStringLiteral("#F48771")));
        cursor.insertText(text, fmt);
    } else {
        cursor.insertText(text);
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
        grayFmt.setForeground(QColor(QStringLiteral("#858585")));
        cursor.insertText(QStringLiteral("\n[%1 more lines]").arg(m_hiddenLineCount), grayFmt);
    } else if (m_hiddenLineCount > 0) {
        // Re-add indicator after removing it, since we're still under the limit
        cursor.movePosition(QTextCursor::End);
        QTextCharFormat grayFmt;
        grayFmt.setForeground(QColor(QStringLiteral("#858585")));
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
