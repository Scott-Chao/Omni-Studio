#include "terminalpanel.h"

#include "terminal/terminalsession.h"
#include "terminal/terminalview.h"
#include "core/thememanager.h"

#include <QDir>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("terminalPanel"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("terminalPanelHeader"));
    header->setFixedHeight(28);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 0, 4, 0);
    headerLayout->setSpacing(4);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("terminalTabs"));
    m_tabs->setTabsClosable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setExpanding(false);

    m_newTerminalBtn = new QPushButton(QStringLiteral("+"), header);
    m_newTerminalBtn->setFixedSize(22, 22);
    m_newTerminalBtn->setFlat(true);
    m_newTerminalBtn->setToolTip(tr("New PowerShell terminal"));

    m_closePanelBtn = new QPushButton(QStringLiteral("x"), header);
    m_closePanelBtn->setFixedSize(22, 22);
    m_closePanelBtn->setFlat(true);
    m_closePanelBtn->setToolTip(tr("Close terminal panel"));

    headerLayout->addStretch();
    headerLayout->addWidget(m_newTerminalBtn);
    headerLayout->addWidget(m_closePanelBtn);

    mainLayout->addWidget(header);
    mainLayout->addWidget(m_tabs);

    connect(m_newTerminalBtn, &QPushButton::clicked, this, &TerminalPanel::openTerminal);
    connect(m_closePanelBtn, &QPushButton::clicked, this, &TerminalPanel::closeRequested);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &TerminalPanel::closeTerminal);

    ThemeManager::watchTheme(this, &TerminalPanel::refreshStyle);
    refreshStyle();
}

TerminalPanel::~TerminalPanel() = default;

void TerminalPanel::setWorkingDirectoryProvider(std::function<QString()> provider)
{
    m_workingDirectoryProvider = std::move(provider);
}

void TerminalPanel::ensureTerminal()
{
    if (terminalCount() == 0)
        openTerminal();
    if (auto *view = qobject_cast<TerminalView*>(m_tabs->currentWidget()))
        view->setFocus();
}

void TerminalPanel::openTerminal()
{
    QString cwd = workingDirectory();
    TerminalView *view = createTerminalView(cwd);
    int index = m_tabs->addTab(view, tr("PowerShell %1").arg(m_nextTerminalId++));
    m_tabs->setCurrentIndex(index);
    view->setFocus();
    QTimer::singleShot(0, view, [view, cwd]() {
        if (TerminalSession *session = view->session())
            session->start(cwd, view->terminalColumns(), view->terminalRows());
        view->syncTerminalSize();
    });
    QTimer::singleShot(80, view, [view]() {
        view->syncTerminalSize();
    });
    QTimer::singleShot(250, view, [view]() {
        view->syncTerminalSize();
    });
}

int TerminalPanel::terminalCount() const
{
    return m_tabs ? m_tabs->count() : 0;
}

TerminalView *TerminalPanel::createTerminalView(const QString &workingDirectory)
{
    auto *view = new TerminalView(m_tabs);
    auto *session = new TerminalSession(view);
    view->attachSession(session);
    Q_UNUSED(workingDirectory)
    return view;
}

QString TerminalPanel::workingDirectory() const
{
    if (m_workingDirectoryProvider) {
        QString path = m_workingDirectoryProvider();
        if (!path.isEmpty() && QDir(path).exists())
            return path;
    }
    return QDir::homePath();
}

void TerminalPanel::closeTerminal(int index)
{
    QWidget *widget = m_tabs->widget(index);
    if (!widget)
        return;
    m_tabs->removeTab(index);
    widget->deleteLater();
}

void TerminalPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();
    setStyleSheet(QStringLiteral(
        "#terminalPanel { background: %1; }"
        "#terminalPanelHeader { background: %2; border-bottom: 1px solid %3; }"
        "QTabWidget::pane { border: none; background: %1; }"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: %4;"
        "  padding: 4px 10px;"
        "  border: none;"
        "  min-height: 20px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: %2;"
        "  color: %5;"
        "  font-weight: bold;"
        "}"
        "QTabBar::tab:hover { background: %6; color: %5; }"
        "QPushButton { background: transparent; border: none; color: %4; border-radius: 3px; }"
        "QPushButton:hover { background: %6; color: %5; }")
        .arg(tm.color("output.background").name(),
             tm.color("tab.inactiveBackground").name(),
             tm.color("panel.border").name(),
             tm.color("editorLineNumber.foreground").name(),
             tm.color("workbench.foreground").name(),
             tm.color("list.hoverBackground").name()));
}
