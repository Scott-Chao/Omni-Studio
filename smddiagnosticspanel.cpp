#include "smddiagnosticspanel.h"
#include "smdeditor.h"
#include "smdlspmanager.h"
#include "thememanager.h"
#include "codeeditor.h"

#include <QApplication>

// ============================================================
// SmdDiagnosticsPanel
// ============================================================

SmdDiagnosticsPanel::SmdDiagnosticsPanel(SmdEditor *editor, QWidget *parent)
    : QFrame(parent)
    , m_editor(editor)
{
    auto &tm = ThemeManager::instance();
    QString editorBg = tm.color("editor.background").name();
    QString cellFg = tm.color("cell.foreground").name();
    QString panelBorder = tm.color("panel.border").name();
    QString lineFg = tm.color("editorLineNumber.foreground").name();
    QString menuBg = tm.color("menu.background").name();

    setObjectName(QStringLiteral("smdDiagnosticsPanel"));
    setFrameStyle(QFrame::NoFrame);
    setStyleSheet(QStringLiteral(
        "#smdDiagnosticsPanel { background-color: %1; }"
    ).arg(editorBg));
    // Height is controlled by the QSplitter in SmdEditor.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header bar
    auto *headerBar = new QWidget(this);
    headerBar->setObjectName(QStringLiteral("diagnosticsHeaderBar"));
    headerBar->setFixedHeight(28);
    headerBar->setStyleSheet(QStringLiteral(
        "#diagnosticsHeaderBar { background: %1; border-bottom: 1px solid %2; }"
    ).arg(menuBg, panelBorder));
    auto *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(8, 0, 4, 0);
    headerLayout->setSpacing(0);

    auto *titleLabel = new QLabel(QStringLiteral("诊断"), headerBar);
    titleLabel->setObjectName(QStringLiteral("diagnosticsTitleLabel"));
    titleLabel->setStyleSheet(QStringLiteral(
        "#diagnosticsTitleLabel { color: %1; font-size: 11px; font-weight: bold; "
        "background: transparent; border: none; }"
    ).arg(cellFg));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_closeBtn = new QPushButton(QStringLiteral("✕"), headerBar);  // ✕
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setFlat(true);
    m_closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; color: %1; "
        "font-size: 12px; }"
        "QPushButton:hover { color: %2; background: %3; border-radius: 2px; }"
    ).arg(lineFg, cellFg, panelBorder));
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        setVisible(false);
    });
    headerLayout->addWidget(m_closeBtn);

    mainLayout->addWidget(headerBar);

    // Content area with scroll
    auto *contentArea = new QScrollArea(this);
    contentArea->setWidgetResizable(true);
    contentArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: %1; border: none; }"
    ).arg(editorBg));

    auto *contentWidget = new QWidget(contentArea);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_errorSection = new DiagnosticSection(QStringLiteral("错误"),
                                           tm.color("diagnostics.error").name(), 1, contentWidget);
    m_warningSection = new DiagnosticSection(QStringLiteral("警告"),
                                             tm.color("diagnostics.warning").name(), 2, contentWidget);

    m_emptyLabel = new QLabel(QStringLiteral("无诊断信息"), contentWidget);
    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; padding: 8px; }"
    ).arg(ThemeManager::instance().color("editorLineNumber.foreground").name()));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setVisible(false);

    connect(m_errorSection, &DiagnosticSection::lineClicked,
            this, &SmdDiagnosticsPanel::onLineClicked);
    connect(m_warningSection, &DiagnosticSection::lineClicked,
            this, &SmdDiagnosticsPanel::onLineClicked);

    contentLayout->addWidget(m_emptyLabel);
    contentLayout->addWidget(m_errorSection);
    contentLayout->addWidget(m_warningSection);
    contentLayout->addStretch();

    contentArea->setWidget(contentWidget);
    mainLayout->addWidget(contentArea);

    // Debounce timer
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(500);
    connect(m_refreshTimer, &QTimer::timeout, this, &SmdDiagnosticsPanel::refresh);

    ThemeManager::watchTheme(this, &SmdDiagnosticsPanel::refreshStyle);
}

void SmdDiagnosticsPanel::refresh()
{
    int cellIndex = m_editor->activeCellIndex();
    if (cellIndex < 0) {
        clear();
        return;
    }

    SmdLspManager *lsp = m_editor->lspManager();
    if (!lsp) {
        clear();
        return;
    }

    QList<SmdDiagnostic> diags = lsp->diagnosticsForCell(cellIndex);

    m_errorSection->setDiagnostics(cellIndex, diags);
    m_warningSection->setDiagnostics(cellIndex, diags);

    // Hide sections that have no diagnostics of that type.
    m_errorSection->setVisible(m_errorSection->count() > 0);
    m_warningSection->setVisible(m_warningSection->count() > 0);

    bool any = (m_errorSection->count() > 0 || m_warningSection->count() > 0);
    m_emptyLabel->setVisible(!any);
}

void SmdDiagnosticsPanel::scheduleRefresh()
{
    m_refreshTimer->start();
}

void SmdDiagnosticsPanel::clear()
{
    m_refreshTimer->stop();
    m_errorSection->clear();
    m_warningSection->clear();
    m_errorSection->setVisible(false);
    m_warningSection->setVisible(false);
    m_emptyLabel->setVisible(true);
}

void SmdDiagnosticsPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();
    QString editorBg = tm.color("editor.background").name();
    QString cellFg = tm.color("cell.foreground").name();
    QString panelBorder = tm.color("panel.border").name();
    QString lineFg = tm.color("editorLineNumber.foreground").name();
    QString menuBg = tm.color("menu.background").name();

    setStyleSheet(QStringLiteral(
        "#smdDiagnosticsPanel { background-color: %1; }"
        "#diagnosticsHeaderBar { background: %2; border-bottom: 1px solid %3; }"
        "#diagnosticsTitleLabel { color: %4; font-size: 11px; font-weight: bold; "
        "background: transparent; border: none; }"
    ).arg(editorBg, menuBg, panelBorder, cellFg));

    m_closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; color: %1; "
        "font-size: 12px; }"
        "QPushButton:hover { color: %2; background: %3; border-radius: 2px; }"
    ).arg(lineFg, cellFg, panelBorder));

    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; padding: 8px; }"
    ).arg(lineFg));

    m_errorSection->refreshStyle();
    m_warningSection->refreshStyle();

    scheduleRefresh();
}

void SmdDiagnosticsPanel::onLineClicked(int cellIndex, int line)
{
    SmdCell *cell = m_editor->cellAt(cellIndex);
    if (!cell) return;
    CodeEditor *ce = qobject_cast<CodeEditor*>(cell->editorWidget());
    if (!ce) return;

    // setOutlineHighlightLine takes 1-based line, diagnostics use 0-based
    ce->setOutlineHighlightLine(line + 1);

    if (cellIndex != m_editor->activeCellIndex()) {
        m_editor->setActiveCell(cellIndex);
        // setActiveCell defers its ensureWidgetVisible via QTimer::singleShot(0).
        // Defer the scroll so the SmdEditor scroll area settles first.
        QTimer::singleShot(0, this, [this, cellIndex, line]() {
            m_editor->scrollCellToLine(cellIndex, line);
        });
    } else {
        m_editor->scrollCellToLine(cellIndex, line);
    }
}
