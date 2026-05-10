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

    // Timer for line-by-line paste delivery
    m_pasteTimer = new QTimer(this);
    m_pasteTimer->setTimerType(Qt::PreciseTimer);
    m_pasteTimer->setSingleShot(false);
    connect(m_pasteTimer, &QTimer::timeout, this, &OutputPanel::sendNextPasteLine);

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

    m_hideBtn = new QPushButton(tr("隐藏"), this);
    m_hideBtn->setFixedWidth(60);

    QHBoxLayout *toolLayout = new QHBoxLayout;
    toolLayout->setContentsMargins(4, 2, 4, 2);
    toolLayout->addWidget(m_statusLabel);
    toolLayout->addStretch();
    toolLayout->addWidget(m_stopBtn);
    toolLayout->addWidget(m_clearBtn);
    toolLayout->addWidget(m_hideBtn);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_outputEdit);
    mainLayout->addLayout(toolLayout);

    connect(m_stopBtn, &QPushButton::clicked, this, &OutputPanel::stopRequested);
    connect(m_clearBtn, &QPushButton::clicked, this, &OutputPanel::clearOutput);
    connect(m_hideBtn, &QPushButton::clicked, this, &OutputPanel::hideRequested);
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
        // Output program text exactly as produced — no artificial newlines.
        // Programs that don't output \\n (e.g. cout << i << "---") render
        // contiguously, matching raw terminal behavior.
        cursor.insertText(text);
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
        m_outputEdit->setTextInteractionFlags(Qt::TextEditorInteraction);
    } else {
        m_acceptingInput = false;
        m_inputBuffer.clear();
        m_pasteQueue.clear();
        m_pasteTimer->stop();
        m_outputEdit->setReadOnly(true);
        // 允许选中文本，但不可编辑
        QPlainTextEdit *edit = m_outputEdit;
        edit->setTextInteractionFlags(Qt::TextSelectableByMouse
                                       | Qt::TextSelectableByKeyboard);
    }
}

void OutputPanel::enableTextSelection(bool enabled)
{
    if (enabled) {
        m_outputEdit->setTextInteractionFlags(Qt::TextSelectableByMouse
                                               | Qt::TextSelectableByKeyboard);
    } else {
        m_outputEdit->setTextInteractionFlags(Qt::NoTextInteraction);
    }
}

bool OutputPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_outputEdit) {
        // Block all key events when not accepting input (e.g. during compilation)
        if (!m_acceptingInput && event->type() == QEvent::KeyPress)
            return true;

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

            // Ignore keys while paste queue is being delivered
            if (!m_pasteQueue.isEmpty()) {
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

    // Normalize line endings
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));

    // Discard any partial buffer — paste replaces pending input
    m_inputBuffer.clear();

    m_pasteEndsWithNewline = text.endsWith(QLatin1Char('\n'));

    // Split into individual lines for line-by-line delivery
    m_pasteQueue = text.split(QStringLiteral("\n"));
    // Remove trailing empty entry that split produces when text ends with \n
    while (!m_pasteQueue.isEmpty() && m_pasteQueue.last().isEmpty())
        m_pasteQueue.removeLast();

    if (m_pasteQueue.isEmpty())
        return;

    // Send first line immediately (echo + send), rest via timer
    sendNextPasteLine();
    if (!m_pasteQueue.isEmpty()) {
        m_pasteTimer->start(20);
    }
}

void OutputPanel::sendNextPasteLine()
{
    if (!m_acceptingInput || m_pasteQueue.isEmpty()) {
        m_pasteQueue.clear();
        m_pasteTimer->stop();
        return;
    }

    QString line = m_pasteQueue.takeFirst();

    // Stop timer before echo/send to prevent re-entrancy
    m_pasteTimer->stop();

    bool isLastLine = m_pasteQueue.isEmpty();

    if (isLastLine && !m_pasteEndsWithNewline) {
        // Last line without trailing newline: echo content, put into input
        // buffer so user can edit before pressing Enter.
        QTextCursor cursor = m_outputEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(line);
        m_outputEdit->setTextCursor(cursor);
        m_outputEdit->ensureCursorVisible();
        m_inputBuffer = line;
        return;
    }

    // Normal line: echo and send
    QTextCursor cursor = m_outputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(line + QStringLiteral("\n"));
    m_outputEdit->setTextCursor(cursor);
    m_outputEdit->ensureCursorVisible();

    emit sendInput(line);  // writeInput appends \n

    // Force I/O processing so program output is displayed
    // before the next echoed line (giving interleaved output)
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    if (!m_pasteQueue.isEmpty()) {
        m_pasteTimer->start(20);
    }
}
