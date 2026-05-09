#include "outputpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QTextCursor>
#include <QKeyEvent>
#include <QClipboard>
#include <QApplication>
#include <QCoreApplication>
#include <QMenu>

OutputPanel::OutputPanel(QWidget *parent)
    : QWidget(parent)
{
    m_outputEdit = new QPlainTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setMaximumBlockCount(10000);
    m_outputEdit->setContextMenuPolicy(Qt::CustomContextMenu);

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
    m_outputEdit->installEventFilter(this);

    // Right-click: paste when accepting input, otherwise show copy menu
    connect(m_outputEdit, &QWidget::customContextMenuRequested, this, [this](const QPoint &) {
        if (m_acceptingInput) {
            pasteToInput();
        } else {
            QMenu menu(this);
            menu.addAction(tr("复制"), m_outputEdit, &QPlainTextEdit::copy);
            menu.addAction(tr("全选"), m_outputEdit, &QPlainTextEdit::selectAll);
            menu.exec(QCursor::pos());
        }
    });

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
    if (running) {
        m_acceptingInput = true;
        m_inputBuffer.clear();
        m_outputEdit->setReadOnly(false);
    } else {
        m_acceptingInput = false;
        m_inputBuffer.clear();
        m_outputEdit->setReadOnly(true);
    }
}

bool OutputPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_outputEdit && m_acceptingInput) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

            // Ctrl+C: stop the running process
            if (keyEvent->key() == Qt::Key_C && keyEvent->modifiers() == Qt::ControlModifier) {
                emit stopRequested();
                return true;
            }

            // Ctrl+V: paste (multi-line aware)
            if (keyEvent->matches(QKeySequence::Paste)) {
                pasteToInput();
                return true;
            }

            QTextCursor cursor = m_outputEdit->textCursor();
            cursor.movePosition(QTextCursor::End);

            // Enter: send buffered input to process
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                emit sendInput(m_inputBuffer);
                m_inputBuffer.clear();
                cursor.insertText(QStringLiteral("\n"));
                m_outputEdit->setTextCursor(cursor);
                m_outputEdit->ensureCursorVisible();
                return true;
            }

            // Backspace: remove last char from buffer and display
            if (keyEvent->key() == Qt::Key_Backspace) {
                if (!m_inputBuffer.isEmpty()) {
                    m_inputBuffer.chop(1);
                    cursor.deletePreviousChar();
                    m_outputEdit->setTextCursor(cursor);
                    m_outputEdit->ensureCursorVisible();
                }
                return true;
            }

            // Printable text (including space)
            QString text = keyEvent->text();
            if (!text.isEmpty() && text.at(0).isPrint()) {
                m_inputBuffer += text;
                cursor.insertText(text);
                m_outputEdit->setTextCursor(cursor);
                m_outputEdit->ensureCursorVisible();
                return true;
            }

            // Block all other keys (arrows, delete, home, end, etc.)
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void OutputPanel::pasteToInput()
{
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty())
        return;

    // Normalize line endings and split
    QStringList lines = text.replace(QStringLiteral("\r\n"), QStringLiteral("\n")).split(QLatin1Char('\n'));

    for (int i = 0; i < lines.size(); ++i) {
        QTextCursor cursor = m_outputEdit->textCursor();
        cursor.movePosition(QTextCursor::End);

        if (i < lines.size() - 1) {
            // Complete line: echo, send to process, then yield event loop
            m_inputBuffer += lines[i];
            cursor.insertText(lines[i] + QStringLiteral("\n"));
            emit sendInput(m_inputBuffer);
            m_inputBuffer.clear();
            m_outputEdit->setTextCursor(cursor);
            m_outputEdit->ensureCursorVisible();
            QCoreApplication::processEvents();
        } else {
            // Last line (may be incomplete): echo, buffer, cursor stays at end
            m_inputBuffer += lines[i];
            cursor.insertText(lines[i]);
            m_outputEdit->setTextCursor(cursor);
            m_outputEdit->ensureCursorVisible();
        }
    }
}
