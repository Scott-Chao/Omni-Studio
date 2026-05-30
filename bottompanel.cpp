#include "bottompanel.h"
#include "outputpanel.h"
#include "diagnosticsection.h"
#include "codeeditor.h"
#include "utilities.h"
#include "thememanager.h"
#include "tabbuttongroup.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>

BottomPanel::BottomPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bottomPanel"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- Header bar ----
    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(28);
    auto *headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(4, 0, 4, 0);
    headerLayout->setSpacing(2);

    m_runTabBtn = new QPushButton(tr("输出"), m_headerBar);
    m_runTabBtn->setFlat(true);
    m_runTabBtn->setCursor(Qt::PointingHandCursor);
    m_runTabBtn->setFixedHeight(22);
    m_runTabBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    m_diagnosticsTabBtn = new QPushButton(tr("诊断"), m_headerBar);
    m_diagnosticsTabBtn->setFlat(true);
    m_diagnosticsTabBtn->setCursor(Qt::PointingHandCursor);
    m_diagnosticsTabBtn->setFixedHeight(22);
    m_diagnosticsTabBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    headerLayout->addWidget(m_runTabBtn);
    headerLayout->addWidget(m_diagnosticsTabBtn);
    headerLayout->addStretch();

    m_closeBtn = new QPushButton(QStringLiteral("✕"), m_headerBar);
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setFlat(true);
    connect(m_closeBtn, &QPushButton::clicked, this, &BottomPanel::closeRequested);
    headerLayout->addWidget(m_closeBtn);

    mainLayout->addWidget(m_headerBar);

    // ---- Stacked content ----
    m_stack = new QStackedWidget(this);

    // Page 0: OutputPanel
    m_outputPanel = new OutputPanel(m_stack);
    m_stack->addWidget(m_outputPanel); // index 0

    // Page 1: Diagnostics view
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
            this, [this](int /*cellIndex*/, int line) {
                emit diagnosticsLineClicked(line + 1);
            });
    connect(m_warningSection, &DiagnosticSection::lineClicked,
            this, [this](int /*cellIndex*/, int line) {
                emit diagnosticsLineClicked(line + 1);
            });

    contentLayout->addWidget(m_emptyLabel);
    contentLayout->addWidget(m_errorSection);
    contentLayout->addWidget(m_warningSection);
    contentLayout->addStretch();

    contentArea->setWidget(contentWidget);
    diagLayout->addWidget(contentArea);

    m_stack->addWidget(m_diagnosticsPage); // index 1

    mainLayout->addWidget(m_stack);

    // ---- Tab button group ----
    m_tabGroup = new TabButtonGroup(m_stack, this);
    m_tabGroup->addTab(m_runTabBtn, RunTab);
    m_tabGroup->addTab(m_diagnosticsTabBtn, DiagnosticsTab);
    m_tabGroup->setStyleProvider(&BottomPanel::tabButtonStyle);
    m_tabGroup->setCurrentIndex(RunTab);

    ThemeManager::watchTheme(this, &BottomPanel::refreshStyle);
    refreshStyle();
}

// ── Static tab button style provider ──

QString BottomPanel::tabButtonStyle(int /*index*/, bool active)
{
    auto &tm = ThemeManager::instance();
    if (active) {
        return QStringLiteral(
            "QPushButton { background: %1; border: none; border-radius: 3px; "
            "color: %2; font-size: 11px; font-weight: bold; padding: 2px 10px; }"
            "QPushButton:hover { background: %3; }"
        ).arg(tm.color("titleBar.background").name(),
              tm.color("workbench.foreground").name(),
              tm.color("tab.hoverBackground").name());
    }
    return QStringLiteral(
        "QPushButton { background: transparent; border: none; border-radius: 3px; "
        "color: %1; font-size: 11px; padding: 2px 10px; }"
        "QPushButton:hover { background: %2; color: %3; }"
    ).arg(tm.color("editorLineNumber.foreground").name(),
          tm.color("list.hoverBackground").name(),
          tm.color("workbench.foreground").name());
}

void BottomPanel::setDiagnostics(const QList<SmdDiagnostic> &diagnostics)
{
    int errCount = 0, warnCount = 0;
    for (const auto &d : diagnostics) {
        if (d.severity == 1) errCount++;
        else if (d.severity == 2) warnCount++;
    }
    debugLog(QString("BottomPanel::setDiagnostics: %1 total, %2 errors, %3 warnings")
        .arg(diagnostics.size()).arg(errCount).arg(warnCount));
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

void BottomPanel::rebuildDiagnostics()
{
    // Use cellIndex = 0 for flat-file diagnostics
    m_errorSection->setDiagnostics(0, m_diagnostics);
    m_warningSection->setDiagnostics(0, m_diagnostics);

    m_errorSection->setVisible(m_errorSection->count() > 0);
    m_warningSection->setVisible(m_warningSection->count() > 0);

    bool any = (m_errorSection->count() > 0 || m_warningSection->count() > 0);
    m_emptyLabel->setVisible(!any);
}

void BottomPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QStringLiteral(
        "#bottomPanel { background-color: %1; }"
    ).arg(tm.color("output.background").name()));

    m_headerBar->setStyleSheet(QStringLiteral(
        "background: %1; border-bottom: 1px solid %2;"
    ).arg(tm.color("tab.inactiveBackground").name(),
          tm.color("panel.border").name()));

    m_closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; color: %1; font-size: 12px; }"
        "QPushButton:hover { color: %2; background: %3; border-radius: 2px; }"
    ).arg(tm.color("editorLineNumber.foreground").name(),
          tm.color("workbench.foreground").name(),
          tm.color("list.hoverBackground").name()));

    m_diagnosticsPage->setStyleSheet(QStringLiteral(
        "background-color: %1;"
    ).arg(tm.color("output.background").name()));

    m_diagScrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: %1; border: none; }"
    ).arg(tm.color("output.background").name()));

    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; padding: 8px; }"
    ).arg(tm.color("editorLineNumber.foreground").name()));

    // Refresh diagnostic section headers + border color, then rebuild entry
    // widgets so existing entries pick up the new theme colors.
    m_errorSection->refreshStyle();
    m_warningSection->refreshStyle();
    rebuildDiagnostics();

    m_tabGroup->refreshStyles();
}
