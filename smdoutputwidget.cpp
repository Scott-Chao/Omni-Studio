#include "smdoutputwidget.h"
#include <QFont>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>

SmdOutputWidget::SmdOutputWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_outputEdit = new QPlainTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setFocusPolicy(Qt::NoFocus);
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
    QStringList lines = text.split(QLatin1Char('\n'));
    if (lines.size() > kMaxOutputLines) {
        lines = lines.mid(0, kMaxOutputLines);
        lines.append(QStringLiteral("[... more lines]"));
    }
    m_outputEdit->setPlainText(lines.join(QLatin1Char('\n')));
    setVisible(true);
}

void SmdOutputWidget::appendText(const QString &text, bool isStderr)
{
    setVisible(true);
    QTextCursor cursor = m_outputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    if (isStderr) {
        QTextCharFormat fmt;
        fmt.setForeground(QColor(QStringLiteral("#F48771")));
        cursor.insertText(text, fmt);
    } else {
        cursor.insertText(text);
    }

    // Enforce max output lines
    int blockCount = m_outputEdit->document()->blockCount();
    if (blockCount > kMaxOutputLines) {
        // Remove excess lines from the beginning
        cursor.movePosition(QTextCursor::Start);
        for (int i = 0; i < blockCount - kMaxOutputLines; ++i) {
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.deleteChar(); // remove the newline
        }
        // Prepend truncation indicator
        cursor.movePosition(QTextCursor::Start);
        QTextCharFormat grayFmt;
        grayFmt.setForeground(QColor(QStringLiteral("#858585")));
        cursor.insertText(QStringLiteral("[... more lines]\n"), grayFmt);
    }

    m_outputEdit->ensureCursorVisible();
}

void SmdOutputWidget::clearOutput()
{
    m_outputEdit->clear();
    setVisible(false);
}

QString SmdOutputWidget::outputText() const
{
    return m_outputEdit->toPlainText();
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
