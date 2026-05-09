#include "outputpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QTextCursor>

OutputPanel::OutputPanel(QWidget *parent)
    : QWidget(parent)
{
    m_outputEdit = new QPlainTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setMaximumBlockCount(10000);

    QFont monoFont(QStringLiteral("Consolas"), 10);
    monoFont.setStyleHint(QFont::Monospace);
    m_outputEdit->setFont(monoFont);

    m_outputEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "  background-color: #1E1E1E;"
        "  color: #D4D4D4;"
        "  selection-background-color: #264F78;"
        "  border: none;"
        "}"));

    m_outputEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_outputEdit->setFrameShape(QFrame::NoFrame);

    // Status bar row
    m_statusLabel = new QLabel(tr("就绪"), this);

    m_stopBtn = new QPushButton(tr("终止"), this);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setFixedWidth(60);

    m_clearBtn = new QPushButton(tr("清除"), this);
    m_clearBtn->setFixedWidth(60);

    QHBoxLayout *toolLayout = new QHBoxLayout;
    toolLayout->setContentsMargins(4, 2, 4, 2);
    toolLayout->addWidget(m_statusLabel);
    toolLayout->addStretch();
    toolLayout->addWidget(m_stopBtn);
    toolLayout->addWidget(m_clearBtn);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_outputEdit);
    mainLayout->addLayout(toolLayout);

    connect(m_stopBtn, &QPushButton::clicked, this, &OutputPanel::stopRequested);
    connect(m_clearBtn, &QPushButton::clicked, this, &OutputPanel::clearOutput);
}

void OutputPanel::appendOutput(const QString &text, bool isStderr)
{
    if (text.isEmpty())
        return;

    QTextCursor cursor = m_outputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    if (isStderr) {
        cursor.insertHtml(
            QStringLiteral("<span style=\"color:#F48771;\">%1</span><br>")
                .arg(text.toHtmlEscaped()));
    } else {
        cursor.insertText(text + QStringLiteral("\n"));
    }

    // Auto-scroll to bottom
    m_outputEdit->setTextCursor(cursor);
    m_outputEdit->ensureCursorVisible();
}

void OutputPanel::clearOutput()
{
    m_outputEdit->clear();
}

void OutputPanel::setStatus(const QString &status, bool isError)
{
    m_statusLabel->setText(status);
    m_statusLabel->setStyleSheet(
        isError ? QStringLiteral("color: #F48771; font-weight: bold; padding: 2px 4px;")
                : QStringLiteral("color: #6A9955; font-weight: bold; padding: 2px 4px;"));
}

void OutputPanel::setRunning(bool running)
{
    m_stopBtn->setEnabled(running);
}
