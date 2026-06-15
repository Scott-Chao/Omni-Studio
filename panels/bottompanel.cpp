#include "bottompanel.h"

#include "core/thememanager.h"
#include "editor/codeeditor.h"
#include "editor/diagnosticsection.h"
#include "panels/runterminalpanel.h"
#include "panels/submissionpanel.h"
#include "panels/terminalpanel.h"
#include "widgets/tabbuttongroup.h"
#include "core/utilities.h"

#include <QIcon>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

BottomPanel::BottomPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bottomPanel"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(28);
    auto *headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(6, 0, 6, 0);
    headerLayout->setSpacing(4);

    m_diagnosticsTabBtn = new QPushButton(tr("诊断"), m_headerBar);
    m_terminalTabBtn = new QPushButton(tr("终端"), m_headerBar);
    m_judgeTabBtn = new QPushButton(tr("评测"), m_headerBar);
    for (auto *button : {m_diagnosticsTabBtn, m_terminalTabBtn, m_judgeTabBtn}) {
        button->setFlat(true);
        button->setCursor(Qt::PointingHandCursor);
        QFont boldFont = button->font();
        boldFont.setBold(true);
        button->setFixedWidth(QFontMetrics(boldFont).horizontalAdvance(button->text()) + 24);
        button->setFixedHeight(24);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        headerLayout->addWidget(button);
    }

    headerLayout->addStretch();

    m_newTerminalBtn = new QPushButton(QStringLiteral("+"), m_headerBar);
    m_newTerminalBtn->setFixedSize(28, 24);
    m_newTerminalBtn->setFlat(true);
    m_newTerminalBtn->setCursor(Qt::PointingHandCursor);
    m_newTerminalBtn->setToolTip(tr("新建终端"));
    headerLayout->addWidget(m_newTerminalBtn);

    m_closeBtn = new QPushButton(m_headerBar);
    m_closeBtn->setIcon(QIcon(QStringLiteral(":/icons/close")));
    m_closeBtn->setIconSize(QSize(16, 16));
    m_closeBtn->setFixedSize(28, 24);
    m_closeBtn->setFlat(true);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip(tr("关闭面板"));
    connect(m_closeBtn, &QPushButton::clicked, this, &BottomPanel::closeRequested);
    headerLayout->addWidget(m_closeBtn);

    mainLayout->addWidget(m_headerBar);

    m_stack = new QStackedWidget(this);

    m_diagnosticsPage = new QWidget(m_stack);
    auto *diagLayout = new QVBoxLayout(m_diagnosticsPage);
    diagLayout->setContentsMargins(0, 0, 0, 0);
    diagLayout->setSpacing(0);

    auto *contentArea = new QScrollArea(m_diagnosticsPage);
    m_diagScrollArea = contentArea;
    contentArea->setWidgetResizable(true);
    contentArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *contentWidget = new QWidget(contentArea);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_errorSection = new DiagnosticSection(tr("错误"),
                                           ThemeManager::instance().color("diagnostics.error").name(),
                                           1, contentWidget);
    m_warningSection = new DiagnosticSection(tr("警告"),
                                             ThemeManager::instance().color("diagnostics.warning").name(),
                                             2, contentWidget);

    m_emptyLabel = new QLabel(tr("无诊断信息"), contentWidget);
    m_emptyLabel->setAlignment(Qt::AlignCenter);

    connect(m_errorSection, &DiagnosticSection::lineClicked,
            this, [this](int, int line) { emit diagnosticsLineClicked(line + 1); });
    connect(m_warningSection, &DiagnosticSection::lineClicked,
            this, [this](int, int line) { emit diagnosticsLineClicked(line + 1); });

    contentLayout->addWidget(m_emptyLabel);
    contentLayout->addWidget(m_errorSection);
    contentLayout->addWidget(m_warningSection);
    contentLayout->addStretch();

    contentArea->setWidget(contentWidget);
    diagLayout->addWidget(contentArea);
    m_stack->addWidget(m_diagnosticsPage);

    m_terminalPanel = new TerminalPanel(m_stack);
    m_stack->addWidget(m_terminalPanel);

    m_submitResultPanel = new SubmitResultPanel(m_stack);
    m_stack->addWidget(m_submitResultPanel);

    mainLayout->addWidget(m_stack);

    m_tabGroup = new TabButtonGroup(m_stack, this);
    m_tabGroup->addTab(m_diagnosticsTabBtn, DiagnosticsTab);
    m_tabGroup->addTab(m_terminalTabBtn, TerminalTab);
    m_tabGroup->addTab(m_judgeTabBtn, JudgeTab);
    m_tabGroup->setStyleProvider(&BottomPanel::tabButtonStyle);
    m_tabGroup->setCurrentIndex(TerminalTab);

    connect(m_tabGroup, &TabButtonGroup::currentChanged, this, [this](int index) {
        m_newTerminalBtn->setVisible(index == TerminalTab);
    });
    connect(m_newTerminalBtn, &QPushButton::clicked, this, [this]() {
        if (m_terminalPanel)
            m_terminalPanel->openTerminal();
    });
    connect(m_submitResultPanel, &SubmitResultPanel::hideRequested, this, [this]() {
        hide();
    });

    ThemeManager::watchTheme(this, &BottomPanel::refreshStyle);
    refreshStyle();
}

QString BottomPanel::tabButtonStyle(int, bool active)
{
    auto &tm = ThemeManager::instance();
    if (active) {
        return QStringLiteral(
            "QPushButton { background: %1; border: none; border-radius: 3px; "
            "color: %2; font-size: 11px; font-weight: bold; padding: 2px 10px; }"
            "QPushButton:hover { background: %3; }")
            .arg(tm.color("titleBar.background").name(),
                 tm.color("workbench.foreground").name(),
                 tm.color("tab.hoverBackground").name());
    }
    return QStringLiteral(
        "QPushButton { background: transparent; border: none; border-radius: 3px; "
        "color: %1; font-size: 11px; padding: 2px 10px; }"
        "QPushButton:hover { background: %2; color: %3; }")
        .arg(tm.color("editorLineNumber.foreground").name(),
             tm.color("list.hoverBackground").name(),
             tm.color("workbench.foreground").name());
}

void BottomPanel::setDiagnostics(const QList<SmdDiagnostic> &diagnostics)
{
    m_diagnostics = diagnostics;
    rebuildDiagnostics();
}

void BottomPanel::clearDiagnostics()
{
    m_diagnostics.clear();
    rebuildDiagnostics();
}

void BottomPanel::setCurrentEditor(CodeEditor *editor)
{
    m_currentEditor = editor;
}

RunTerminalPanel *BottomPanel::runTerminal() const
{
    return m_terminalPanel ? m_terminalPanel->runTerminal() : nullptr;
}

void BottomPanel::showTerminalTab()
{
    m_tabGroup->setCurrentIndex(TerminalTab);
}

void BottomPanel::showSubmissionResult(const SubmissionResult &result)
{
    if (!m_submitResultPanel)
        return;

    const bool shouldResize = !property("bottomPanelSizedOnce").toBool();
    m_submitResultPanel->showResult(result);
    setVisible(true);
    showJudgeTab();

    QTimer::singleShot(0, this, [this, shouldResize]() {
        if (!shouldResize)
            return;
        auto *splitter = qobject_cast<QSplitter*>(parentWidget());
        if (!splitter)
            return;
        const int totalHeight = splitter->height();
        if (totalHeight <= 0)
            return;
        const int panelHeight = totalHeight / 3;
        splitter->setSizes({totalHeight - panelHeight, panelHeight});
        setProperty("bottomPanelSizedOnce", true);
    });
}

void BottomPanel::setWorkingDirectoryProvider(std::function<QString()> provider)
{
    if (m_terminalPanel)
        m_terminalPanel->setWorkingDirectoryProvider(std::move(provider));
}

void BottomPanel::rebuildDiagnostics()
{
    m_errorSection->setDiagnostics(0, m_diagnostics);
    m_warningSection->setDiagnostics(0, m_diagnostics);

    m_errorSection->setVisible(m_errorSection->count() > 0);
    m_warningSection->setVisible(m_warningSection->count() > 0);

    const bool any = (m_errorSection->count() > 0 || m_warningSection->count() > 0);
    m_emptyLabel->setVisible(!any);
}

void BottomPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QStringLiteral(
        "#bottomPanel { background-color: %1; }")
        .arg(tm.color("output.background").name()));

    m_headerBar->setStyleSheet(QStringLiteral(
        "background: %1; border-bottom: 1px solid %2;")
        .arg(tm.color("tab.inactiveBackground").name(),
             tm.color("panel.border").name()));

    const QColor iconColor = tm.color("editorLineNumber.foreground");
    m_closeBtn->setIcon(IconUtils::coloredSvgIcon(":/icons/close", iconColor, 16));

    const QString iconButtonStyle = QStringLiteral(
        "QPushButton { background: transparent; border: none; border-radius: 3px; "
        "color: %1; font-size: 16px; font-weight: 500; padding-bottom: 2px; }"
        "QPushButton:hover { background: %2; color: %3; }")
        .arg(iconColor.name(),
             tm.color("button.hoverBackground").name(),
             tm.color("workbench.foreground").name());
    const QString closeButtonStyle = QStringLiteral(
        "QPushButton { background: transparent; border: none; border-radius: 3px; }"
        "QPushButton:hover { background: #E81123; }");
    m_newTerminalBtn->setStyleSheet(iconButtonStyle);
    m_closeBtn->setStyleSheet(closeButtonStyle);

    m_diagnosticsPage->setStyleSheet(QStringLiteral(
        "background-color: %1;")
        .arg(tm.color("output.background").name()));

    m_diagScrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: %1; border: none; }")
        .arg(tm.color("output.background").name()));

    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; padding: 8px; }")
        .arg(tm.color("editorLineNumber.foreground").name()));

    m_errorSection->refreshStyle();
    m_warningSection->refreshStyle();
    rebuildDiagnostics();

    m_tabGroup->refreshStyles();
    m_newTerminalBtn->setVisible(currentTab() == TerminalTab);
}
