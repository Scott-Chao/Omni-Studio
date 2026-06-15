#include "terminalpanel.h"

#include "runterminalpanel.h"
#include "terminal/terminalsession.h"
#include "terminal/terminalview.h"
#include "core/thememanager.h"

#include <QDir>
#include <QFontMetrics>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

class StableWidthTabBar : public QTabBar
{
public:
    explicit StableWidthTabBar(QWidget *parent = nullptr)
        : QTabBar(parent)
    {
    }

protected:
    QSize tabSizeHint(int index) const override
    {
        QSize size = QTabBar::tabSizeHint(index);

        const QString text = tabText(index);
        if (!text.isEmpty()) {
            QFont normalFont = font();
            normalFont.setBold(false);
            QFont boldFont = normalFont;
            boldFont.setBold(true);

            const int normalWidth = QFontMetrics(normalFont).horizontalAdvance(text);
            const int boldWidth = QFontMetrics(boldFont).horizontalAdvance(text);
            if (boldWidth > normalWidth)
                size.rwidth() += boldWidth - normalWidth;
        }

        return size;
    }
};

class TerminalTabWidget : public QTabWidget
{
public:
    explicit TerminalTabWidget(QWidget *parent = nullptr)
        : QTabWidget(parent)
    {
        setTabBar(new StableWidthTabBar(this));
    }
};

}

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("terminalPanel"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_tabs = new TerminalTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("terminalTabs"));
    m_tabs->setTabsClosable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setObjectName(QStringLiteral("terminalTabBar"));
    m_tabs->tabBar()->setExpanding(false);
    m_tabs->tabBar()->setMovable(true);

    m_runTerminal = new RunTerminalPanel(this);
    m_runTerminal->hide();

    mainLayout->addWidget(m_tabs);

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
    else if (!qobject_cast<TerminalView*>(m_tabs->currentWidget())) {
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (qobject_cast<TerminalView*>(m_tabs->widget(i))) {
                m_tabs->setCurrentIndex(i);
                break;
            }
        }
    }
    if (auto *view = qobject_cast<TerminalView*>(m_tabs->currentWidget()))
        view->setFocus();
}

void TerminalPanel::openTerminal()
{
    QString cwd = workingDirectory();
    TerminalView *view = createTerminalView(cwd);
    int index = m_tabs->addTab(view, tr("PowerShell"));
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

void TerminalPanel::openCommandTerminal(const QString &title, const QString &command,
                                        const QString &workingDirectory, const QString &reuseKey)
{
    QString cwd = workingDirectory;
    if (cwd.isEmpty() || !QDir(cwd).exists())
        cwd = this->workingDirectory();

    TerminalView *view = nullptr;
    if (!reuseKey.isEmpty()) {
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *candidate = qobject_cast<TerminalView*>(m_tabs->widget(i));
            if (candidate && candidate->property("terminalReuseKey").toString() == reuseKey) {
                view = candidate;
                m_tabs->setCurrentIndex(i);
                break;
            }
        }
    }

    if (view) {
        view->setFocus();
        QTimer::singleShot(0, view, [view, command]() {
            if (TerminalSession *session = view->session()) {
                QByteArray data = command.toUtf8();
                data.append('\r');
                session->writeInput(data);
            }
            view->syncTerminalSize();
        });
        return;
    }

    view = createTerminalView(cwd);
    if (!reuseKey.isEmpty())
        view->setProperty("terminalReuseKey", reuseKey);
    int index = m_tabs->addTab(view, title.isEmpty() ? tr("Run %1").arg(m_nextTerminalId++) : title);
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
    QTimer::singleShot(180, view, [view, command]() {
        if (TerminalSession *session = view->session()) {
            QByteArray data = command.toUtf8();
            data.append('\r');
            session->writeInput(data);
        }
        view->syncTerminalSize();
    });
    QTimer::singleShot(300, view, [view]() {
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
        "QTabWidget#terminalTabs::pane { border: none; background: %1; }"
        "QTabBar#terminalTabBar {"
        "  background: %1;"
        "  border: none;"
        "  widget-animation-duration: 0;"
        "}"
        "QTabBar#terminalTabBar::tab {"
        "  background: %2;"
        "  color: %4;"
        "  padding: 4px 10px;"
        "  border: none;"
        "  border-top: 2px solid transparent;"
        "  border-right: 1px solid %3;"
        "  border-radius: 0px;"
        "  min-height: 20px;"
        "}"
        "QTabBar#terminalTabBar::tab:selected {"
        "  background: %8;"
        "  color: %5;"
        "  font-weight: bold;"
        "  border-top: 2px solid %9;"
        "}"
        "QTabBar#terminalTabBar::tab:hover:!selected {"
        "  background: %6;"
        "  color: %5;"
        "}"
        "QTabBar#terminalTabBar::tab:selected:hover {"
        "  background: %8;"
        "  color: %5;"
        "}"
        "QTabBar#terminalTabBar::close-button {"
        "  image: url(:/icons/close);"
        "  subcontrol-position: right;"
        "  margin: 2px;"
        "  border-radius: 3px;"
        "}"
        "QTabBar#terminalTabBar::close-button:hover {"
        "  background: %7;"
        "}"
        "QPushButton { background: transparent; border: none; color: %4; border-radius: 3px; }"
        "QPushButton:hover { background: %6; color: %5; }")
        .arg(tm.color("output.background").name(),
             tm.color("tab.inactiveBackground").name(),
             tm.color("panel.border").name(),
             tm.color("tab.inactiveForeground").name(),
             tm.color("workbench.foreground").name(),
             tm.color("button.hoverBackground").name(),
             tm.color("titleBar.buttonCloseHover").name(),
             tm.color("tab.activeBackground").name(),
             tm.color("tab.activeIndicator").name()));
    m_tabs->tabBar()->updateGeometry();
    m_tabs->tabBar()->update();
}
