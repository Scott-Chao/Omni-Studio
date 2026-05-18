#include "smddiagnosticspanel.h"
#include "smdeditor.h"

#include <QApplication>

// ============================================================
// DiagnosticSection
// ============================================================

DiagnosticSection::DiagnosticSection(const QString &title, const QString &borderColor,
                                     int severity, QWidget *parent)
    : QWidget(parent)
    , m_borderColor(borderColor)
    , m_title(title)
    , m_severity(severity)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_headerLabel = new QLabel(this);
    m_headerLabel->setCursor(Qt::PointingHandCursor);
    m_headerLabel->setTextFormat(Qt::RichText);
    m_headerLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e0e0e0; font-size: 11px; font-weight: bold; "
        "padding: 4px 8px; border-bottom: 1px solid #3c3c3c; }"
    ));

    m_contentWidget = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    layout->addWidget(m_headerLabel);
    layout->addWidget(m_contentWidget);

    connect(m_headerLabel, &QLabel::linkActivated, this, [this](const QString &) {
        setExpanded(!m_expanded);
    });

    setExpanded(true);
}

void DiagnosticSection::setDiagnostics(int cellIndex, const QList<SmdDiagnostic> &diags)
{
    m_cellIndex = cellIndex;

    // Clear existing entries
    QLayoutItem *child;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // Filter diagnostics by this section's severity
    QList<SmdDiagnostic> filtered;
    for (const auto &d : diags) {
        if (d.severity == m_severity)
            filtered.append(d);
    }
    m_count = filtered.size();

    // Sort by line number
    std::sort(filtered.begin(), filtered.end(),
              [](const SmdDiagnostic &a, const SmdDiagnostic &b) {
                  return a.startLine < b.startLine;
              });

    for (const auto &d : filtered) {
        QString entryText = QStringLiteral("<span style=\"color:#858585;\">行 %1:</span> "
                                           "<span style=\"color:#D4D4D4;\">%2</span>")
                                .arg(d.startLine + 1)
                                .arg(d.message.toHtmlEscaped());

        auto *entry = new QLabel(entryText, m_contentWidget);
        entry->setWordWrap(true);
        entry->setCursor(Qt::PointingHandCursor);
        entry->setStyleSheet(QStringLiteral(
            "QLabel { color: #D4D4D4; font-size: 11px; padding: 3px 8px 3px 12px; "
            "border-left: 2px solid %1; background: #252526; }")
            .arg(m_borderColor));
        entry->setTextFormat(Qt::RichText);

        int line = d.startLine;
        int ci = cellIndex;
        connect(entry, &QLabel::linkActivated, this, [this, ci, line](const QString &) {
            emit lineClicked(ci, line);
        });

        m_contentLayout->addWidget(entry);
    }

    setExpanded(m_expanded);  // update header text and visibility
}

void DiagnosticSection::clear()
{
    QLayoutItem *child;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    m_count = 0;
    m_cellIndex = -1;
    setExpanded(m_expanded);
}

void DiagnosticSection::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_contentWidget->setVisible(expanded);

    // ▾ / ▸ are a matched pair from Unicode Geometric Shapes — same visual
    // weight in most monospace fonts, unlike the mismatched ▼/▶ pair.
    QString indicator = expanded ? QStringLiteral("▾")
                                 : QStringLiteral("▸");
    m_headerLabel->setText(QStringLiteral(
        "<a href=\"toggle\" style=\"color:#e0e0e0;text-decoration:none;"
        "font-weight:bold;\">%1 %2 (%3)</a>")
        .arg(indicator, m_title)
        .arg(m_count));
}

// ============================================================
// SmdDiagnosticsPanel
// ============================================================

SmdDiagnosticsPanel::SmdDiagnosticsPanel(SmdEditor *editor, QWidget *parent)
    : QFrame(parent)
    , m_editor(editor)
{
    setObjectName(QStringLiteral("smdDiagnosticsPanel"));
    setFrameStyle(QFrame::NoFrame);
    setStyleSheet(QStringLiteral(
        "#smdDiagnosticsPanel { background-color: #1E1E1E; }"
    ));
    // Height is controlled by the QSplitter in SmdEditor.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header bar
    auto *headerBar = new QWidget(this);
    headerBar->setFixedHeight(28);
    headerBar->setStyleSheet(QStringLiteral(
        "background: #2d2d2d; border-bottom: 1px solid #3c3c3c;"
    ));
    auto *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(8, 0, 4, 0);
    headerLayout->setSpacing(0);

    auto *titleLabel = new QLabel(QStringLiteral("诊断"), headerBar);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e0e0e0; font-size: 11px; font-weight: bold; "
        "background: transparent; border: none; }"
    ));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_closeBtn = new QPushButton(QStringLiteral("✕"), headerBar);  // ✕
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setFlat(true);
    m_closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; color: #858585; "
        "font-size: 12px; }"
        "QPushButton:hover { color: #e0e0e0; background: #3c3c3c; border-radius: 2px; }"
    ));
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
        "QScrollArea { background: #1E1E1E; border: none; }"
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

void SmdDiagnosticsPanel::onLineClicked(int cellIndex, int line)
{
    m_editor->setActiveCell(cellIndex);
    m_editor->setActiveCellCursor(line, 0);
    // Focus the cell's editor widget
    SmdCell *cell = m_editor->cellAt(cellIndex);
    if (cell && cell->editorWidget())
        cell->editorWidget()->setFocus();
}
