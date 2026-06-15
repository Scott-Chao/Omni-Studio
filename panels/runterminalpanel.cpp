#include "runterminalpanel.h"

#include "core/thememanager.h"
#include "terminal/terminalview.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

RunTerminalPanel::RunTerminalPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("runTerminalPanel"));

    m_view = new TerminalView(this);
    m_view->setLocalEchoEnabled(false);

    m_stopBtn = new QPushButton(tr("停止"), this);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setFixedWidth(64);

    m_clearBtn = new QPushButton(tr("清空"), this);
    m_clearBtn->setFixedWidth(64);

    auto *tools = new QHBoxLayout;
    tools->setContentsMargins(4, 2, 4, 2);
    tools->addStretch();
    tools->addWidget(m_stopBtn);
    tools->addWidget(m_clearBtn);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view, 1);
    layout->addLayout(tools);

    connect(m_stopBtn, &QPushButton::clicked, this, &RunTerminalPanel::stopRequested);
    connect(m_clearBtn, &QPushButton::clicked, this, &RunTerminalPanel::clearOutput);
    connect(m_view, &TerminalView::inputGenerated, this, [this](const QByteArray &data) {
        if (!m_acceptingInput || data.isEmpty())
            return;
        emit sendRawInput(QString::fromUtf8(data));
    });

    ThemeManager::watchTheme(this, &RunTerminalPanel::refreshStyle);
    refreshStyle();
}

void RunTerminalPanel::appendOutput(const QString &text, bool isStderr)
{
    if (text.isEmpty())
        return;

    if (isStderr)
        m_view->appendTerminalData(QStringLiteral("\x1b[31m").toUtf8());
    m_view->appendTerminalData(text.toUtf8());
    if (isStderr)
        m_view->appendTerminalData(QStringLiteral("\x1b[0m").toUtf8());
}

void RunTerminalPanel::clearOutput()
{
    m_view->clearTerminal();
}

void RunTerminalPanel::setStatus(const QString &status, bool isError)
{
    if (status.isEmpty())
        return;

    const QString color = isError ? QStringLiteral("\x1b[31m") : QStringLiteral("\x1b[32m");
    m_view->appendTerminalData(QStringLiteral("\r\n%1[%2]\x1b[0m\r\n").arg(color, status).toUtf8());
}

void RunTerminalPanel::setRunning(bool running)
{
    m_running = running;
    if (!running)
        m_acceptingInput = false;
    m_stopBtn->setEnabled(running);
}

void RunTerminalPanel::setInputEnabled(bool enabled)
{
    m_acceptingInput = enabled;
    if (m_view)
        m_view->setLocalEchoEnabled(enabled);
}

void RunTerminalPanel::enableTextSelection(bool enabled)
{
    Q_UNUSED(enabled)
}

void RunTerminalPanel::reloadShortcuts()
{
}

QWidget *RunTerminalPanel::focusTarget() const
{
    return m_view;
}

void RunTerminalPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();
    setStyleSheet(QStringLiteral(
        "#runTerminalPanel { background: %1; }"
        "QPushButton { background: transparent; border: none; color: %2; border-radius: 3px; }"
        "QPushButton:hover { background: %3; color: %4; }"
        "QPushButton:disabled { color: %5; }")
        .arg(tm.color("output.background").name(),
             tm.color("editorLineNumber.foreground").name(),
             tm.color("list.hoverBackground").name(),
             tm.color("workbench.foreground").name(),
             tm.color("panel.border").name()));
}
