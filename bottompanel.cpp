#include "bottompanel.h"
#include "outputpanel.h"
#include "smddiagnosticspanel.h"
#include "codeeditor.h"
#include "debuglog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>

BottomPanel::BottomPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bottomPanel"));
    setStyleSheet(QStringLiteral(
        "#bottomPanel { background-color: #1E1E1E; }"
    ));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- Header bar ----
    auto *headerBar = new QWidget(this);
    headerBar->setFixedHeight(28);
    headerBar->setStyleSheet(QStringLiteral(
        "background: #2d2d2d; border-bottom: 1px solid #3c3c3c;"
    ));
    auto *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(4, 0, 4, 0);
    headerLayout->setSpacing(2);

    m_runTabBtn = new QPushButton(QStringLiteral("输出"), headerBar);
    m_runTabBtn->setFlat(true);
    m_runTabBtn->setCursor(Qt::PointingHandCursor);
    m_runTabBtn->setFixedHeight(22);
    m_runTabBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    m_diagnosticsTabBtn = new QPushButton(QStringLiteral("诊断"), headerBar);
    m_diagnosticsTabBtn->setFlat(true);
    m_diagnosticsTabBtn->setCursor(Qt::PointingHandCursor);
    m_diagnosticsTabBtn->setFixedHeight(22);
    m_diagnosticsTabBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    headerLayout->addWidget(m_runTabBtn);
    headerLayout->addWidget(m_diagnosticsTabBtn);
    headerLayout->addStretch();

    m_closeBtn = new QPushButton(QStringLiteral("✕"), headerBar);
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setFlat(true);
    m_closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; color: #858585; "
        "font-size: 12px; }"
        "QPushButton:hover { color: #e0e0e0; background: #3c3c3c; border-radius: 2px; }"
    ));
    connect(m_closeBtn, &QPushButton::clicked, this, &BottomPanel::closeRequested);
    headerLayout->addWidget(m_closeBtn);

    mainLayout->addWidget(headerBar);

    // ---- Stacked content ----
    m_stack = new QStackedWidget(this);

    // Page 0: OutputPanel
    m_outputPanel = new OutputPanel(m_stack);
    m_stack->addWidget(m_outputPanel); // index 0

    // Page 1: Diagnostics view
    m_diagnosticsPage = new QWidget(m_stack);
    m_diagnosticsPage->setStyleSheet(QStringLiteral("background-color: #1E1E1E;"));

    auto *diagLayout = new QVBoxLayout(m_diagnosticsPage);
    diagLayout->setContentsMargins(0, 0, 0, 0);
    diagLayout->setSpacing(0);

    auto *contentArea = new QScrollArea(m_diagnosticsPage);
    contentArea->setWidgetResizable(true);
    contentArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: #1E1E1E; border: none; }"
        "QScrollBar:vertical {"
        "  background-color: #1E1E1E;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    ));

    auto *contentWidget = new QWidget(contentArea);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_errorSection = new DiagnosticSection(QStringLiteral("错误"),
                                           QStringLiteral("#F44747"), 1, contentWidget);
    m_warningSection = new DiagnosticSection(QStringLiteral("警告"),
                                             QStringLiteral("#CCA700"), 2, contentWidget);

    m_emptyLabel = new QLabel(QStringLiteral("无诊断信息"), contentWidget);
    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #858585; font-size: 11px; padding: 8px; }"
    ));
    m_emptyLabel->setAlignment(Qt::AlignCenter);

    connect(m_errorSection, &DiagnosticSection::lineClicked,
            this, [this](int /*cellIndex*/, int line) {
                emit diagnosticsLineClicked(line);
            });
    connect(m_warningSection, &DiagnosticSection::lineClicked,
            this, [this](int /*cellIndex*/, int line) {
                emit diagnosticsLineClicked(line);
            });

    contentLayout->addWidget(m_emptyLabel);
    contentLayout->addWidget(m_errorSection);
    contentLayout->addWidget(m_warningSection);
    contentLayout->addStretch();

    contentArea->setWidget(contentWidget);
    diagLayout->addWidget(contentArea);

    m_stack->addWidget(m_diagnosticsPage); // index 1

    mainLayout->addWidget(m_stack);

    // ---- Tab button connections ----
    connect(m_runTabBtn, &QPushButton::clicked, this, &BottomPanel::showRunTab);
    connect(m_diagnosticsTabBtn, &QPushButton::clicked, this, &BottomPanel::showDiagnosticsTab);

    updateTabButtonStyles();
    m_stack->setCurrentIndex(0);
}

void BottomPanel::showRunTab()
{
    m_currentTab = RunTab;
    m_stack->setCurrentIndex(0);
    updateTabButtonStyles();
}

void BottomPanel::showDiagnosticsTab()
{
    m_currentTab = DiagnosticsTab;
    m_stack->setCurrentIndex(1);
    updateTabButtonStyles();
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

void BottomPanel::updateTabButtonStyles()
{
    auto activeStyle = QStringLiteral(
        "QPushButton { background: #3c3c3c; border: none; border-radius: 3px; "
        "color: #e0e0e0; font-size: 11px; font-weight: bold; padding: 2px 10px; }"
        "QPushButton:hover { background: #4a4a4a; }"
    );
    auto inactiveStyle = QStringLiteral(
        "QPushButton { background: transparent; border: none; border-radius: 3px; "
        "color: #858585; font-size: 11px; padding: 2px 10px; }"
        "QPushButton:hover { background: #3c3c3c; color: #c0c0c0; }"
    );

    m_runTabBtn->setStyleSheet(m_currentTab == RunTab ? activeStyle : inactiveStyle);
    m_diagnosticsTabBtn->setStyleSheet(m_currentTab == DiagnosticsTab ? activeStyle : inactiveStyle);
}
