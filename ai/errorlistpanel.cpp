#include "errorlistpanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QRegularExpression>
#include <QSet>
#include "thememanager.h"

// ═══════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════

static QString statusColor(const QString &code)
{
    if (code == "WA")  return "#ff8c00";
    if (code == "RE")  return "#e74c3c";
    if (code == "TLE") return "#f39c12";
    if (code == "MLE") return "#9b59b6";
    return "#888888";
}

static QString statusLabel(const QString &code)
{
    if (code == "WA")  return QStringLiteral("答案错误");
    if (code == "RE")  return QStringLiteral("运行时错误");
    if (code == "TLE") return QStringLiteral("超时");
    if (code == "MLE") return QStringLiteral("超内存");
    return code;
}

// ── Section header helper ────────────────────────────────────────

static QWidget *createSectionHeader(const QString &title)
{
    auto &tm = ThemeManager::instance();
    auto *w = new QWidget;
    w->setStyleSheet(QStringLiteral("background-color: %1;")
                     .arg(tm.color("list.hoverBackground").name()));
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(10, 4, 10, 4);

    auto *label = new QLabel(title);
    label->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; font-weight: bold;")
                         .arg(tm.color("editorLineNumber.foreground").name()));
    lay->addWidget(label);
    return w;
}

// ── Collapsible output view ──────────────────────────────────────

static QWidget *createOutputBlock(const QString &title, const QString &content)
{
    auto &tm = ThemeManager::instance();
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(10, 2, 10, 2);
    lay->setSpacing(2);

    auto *titleLbl = new QLabel(title);
    titleLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                            .arg(tm.color("editorLineNumber.foreground").name()));

    auto *tb = new QTextBrowser;
    tb->setPlainText(content);
    tb->setMaximumHeight(120);
    tb->setStyleSheet(QStringLiteral(
        "QTextBrowser {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 3px;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 11px;"
        "  padding: 4px;"
        "}"
    ).arg(tm.color("editor.background").name(),
          tm.color("editor.foreground").name(),
          tm.color("panel.border").name()));

    lay->addWidget(titleLbl);
    lay->addWidget(tb);
    return w;
}

// ═══════════════════════════════════════════════════════════════════
// ErrorListItem — single clickable error record card
// ═══════════════════════════════════════════════════════════════════

ErrorListItem::ErrorListItem(const ErrorRecord &record, QWidget *parent)
    : QWidget(parent), m_record(record)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(64);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(2);

    // ── Row 1: status badge + problem name + arrow ──
    auto *row1 = new QHBoxLayout;
    row1->setSpacing(8);

    auto *badge = new QLabel(m_record.statusCode);
    badge->setFixedSize(36, 20);
    badge->setAlignment(Qt::AlignCenter);
    QString badgeColor = statusColor(m_record.statusCode);
    badge->setStyleSheet(
        QString("background-color: %1; color: #fff; font-size: 10px;"
                " font-weight: bold; border-radius: 3px;").arg(badgeColor));

    auto *nameLabel = new QLabel(m_record.problemName);
    nameLabel->setObjectName(QStringLiteral("errorItemName"));
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    nameLabel->setTextFormat(Qt::PlainText);

    auto *arrow = new QLabel(">");
    arrow->setObjectName(QStringLiteral("errorItemArrow"));
    arrow->setFixedWidth(12);

    row1->addWidget(badge);
    row1->addWidget(nameLabel, 1);
    row1->addWidget(arrow);

    // ── Row 2: source file + time ──
    auto *row2 = new QHBoxLayout;
    row2->setSpacing(12);

    auto *fileLabel = new QLabel(m_record.sourceFile.section('\\', -1).section('/', -1));
    fileLabel->setObjectName(QStringLiteral("errorItemFile"));

    auto *timeLabel = new QLabel(m_record.timestamp.toString("yyyy-MM-dd HH:mm"));
    timeLabel->setObjectName(QStringLiteral("errorItemTime"));

    row2->addWidget(fileLabel);
    row2->addWidget(timeLabel);
    row2->addStretch();

    // ── Row 3: tags (if any) ──
    QLabel *tagsLabel = nullptr;
    if (!m_record.tags.isEmpty()) {
        tagsLabel = new QLabel(m_record.tags.join("  "));
        tagsLabel->setObjectName(QStringLiteral("errorItemTags"));
    }

    layout->addLayout(row1);
    layout->addLayout(row2);
    if (tagsLabel)
        layout->addWidget(tagsLabel);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() { refreshStyle(); });
    refreshStyle();
}

void ErrorListItem::refreshStyle()
{
    m_hoverBg = ThemeManager::instance().color("list.hoverBackground");
    m_borderColor = ThemeManager::instance().color("panel.border");

    auto &tm = ThemeManager::instance();
    setStyleSheet(QStringLiteral(
        "#errorItemName { color: %1; font-size: 12px; font-weight: bold; }"
        "#errorItemArrow { color: %2; font-size: 14px; }"
        "#errorItemFile { color: %3; font-size: 10px; }"
        "#errorItemTime { color: %4; font-size: 10px; }"
        "#errorItemTags { color: %5; font-size: 10px; }"
    ).arg(tm.color("workbench.foreground").name(),
          tm.color("editorLineNumber.foreground").name(),
          tm.color("editorLineNumber.foreground").name(),
          tm.color("tab.inactiveForeground").name(),
          tm.color("activityBar.activeBorder").name()));

    update();
}

void ErrorListItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_record.id);
    QWidget::mousePressEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ErrorListItem::enterEvent(QEnterEvent *event)
#else
void ErrorListItem::enterEvent(QEvent *event)
#endif
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void ErrorListItem::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void ErrorListItem::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_hovered)
        painter.fillRect(rect(), m_hoverBg);

    // Bottom border line
    painter.setPen(QPen(m_borderColor, 1));
    painter.drawLine(0, height() - 1, width(), height() - 1);
}

// ═══════════════════════════════════════════════════════════════════
// Markdown → HTML renderer (simple but handles common AI output patterns)
// ═══════════════════════════════════════════════════════════════════

static QString renderMarkdown(const QString &md)
{
    if (md.isEmpty())
        return {};

    auto &tm = ThemeManager::instance();
    QString codeBg = tm.color("aiAssistant.codeBackground").name();
    QString codeFg = tm.color("aiAssistant.codeForeground").name();
    QString headingFg = tm.color("workbench.foreground").name();
    QString subheadingFg = tm.color("editorLineNumber.foreground").name();

    // Must HTML-escape FIRST so code content (e.g. <iostream>, a < b) isn't
    // misinterpreted as HTML tags by Qt's rich text renderer.
    // Markdown syntax characters (#, *, `) are unaffected by escaping, so the
    // pattern replacements below still work correctly.
    QString html = md.toHtmlEscaped();

    // Normalize line endings
    html.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    html.replace(QStringLiteral("\r"), QStringLiteral("\n"));

    // Code blocks must be handled before inline patterns to avoid
    // contaminating code with heading / bold / etc. replacements.
    // Opening: ```lang\n → <pre>
    html.replace(QRegularExpression(QStringLiteral("```(?:\\w+)?\\n")),
                 QStringLiteral("<pre style='background:%1;padding:8px;"
                                "border-radius:3px;font-size:11px;color:%2'>")
                 .arg(codeBg, codeFg));
    // Closing: remaining ``` → </pre>
    html.replace(QStringLiteral("```"), QStringLiteral("</pre>"));

    // Headings (MultilineOption enables ^/$ per line)
    html.replace(
        QRegularExpression(QStringLiteral("^### (.+)$"),
                           QRegularExpression::MultilineOption),
        QStringLiteral("<h4 style='color:%1;margin:8px 0 4px'>\\1</h4>").arg(subheadingFg));
    html.replace(
        QRegularExpression(QStringLiteral("^## (.+)$"),
                           QRegularExpression::MultilineOption),
        QStringLiteral("<h3 style='color:%1;margin:10px 0 4px'>\\1</h3>").arg(headingFg));

    // Bold **text**
    html.replace(QRegularExpression(QStringLiteral("\\*\\*(.+?)\\*\\*")),
                 QStringLiteral("<b>\\1</b>"));

    // Inline code `code`
    html.replace(QRegularExpression(QStringLiteral("`([^`]+)`")),
                 QStringLiteral("<code style='background:%1;color:%2;"
                                "padding:1px 5px;border-radius:2px;font-size:11px'>"
                                "\\1</code>").arg(codeBg, codeFg));

    // Newlines → <br>
    html.replace(QStringLiteral("\n"), QStringLiteral("<br>"));

    // Clean up: remove <br> that immediately follows block tags
    html.replace(QRegularExpression(QStringLiteral("<br>(</h[34]>)")), QStringLiteral("\\1"));
    html.replace(QRegularExpression(QStringLiteral("<br>(</pre>)")), QStringLiteral("\\1"));

    return html;
}

// ═══════════════════════════════════════════════════════════════════
// ErrorDetailWidget — expanded detail view for a single error record
// ═══════════════════════════════════════════════════════════════════

ErrorDetailWidget::ErrorDetailWidget(const ErrorRecord &record, QWidget *parent)
    : QWidget(parent), m_record(record)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header: metadata row ──
    auto *header = new QWidget;
    header->setObjectName(QStringLiteral("detailHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->setSpacing(16);

    auto *statusLbl = new QLabel(
        QStringLiteral("<b style='color:%1'>%2</b>  %3")
            .arg(statusColor(m_record.statusCode),
                 m_record.statusCode,
                 statusLabel(m_record.statusCode)));
    statusLbl->setObjectName(QStringLiteral("detailStatusLbl"));

    auto *timeLbl = new QLabel(
        QStringLiteral("⏱ %1 ms").arg(m_record.elapsedMs));
    timeLbl->setObjectName(QStringLiteral("detailTimeLbl"));

    auto *memLbl = new QLabel(
        QStringLiteral("💾 %1 KB").arg(m_record.memoryKb));
    memLbl->setObjectName(QStringLiteral("detailMemLbl"));

    auto *fileLbl = new QLabel(m_record.sourceFile);
    fileLbl->setObjectName(QStringLiteral("detailFileLbl"));
    fileLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    headerLayout->addWidget(statusLbl);
    headerLayout->addWidget(timeLbl);
    headerLayout->addWidget(memLbl);
    headerLayout->addWidget(fileLbl, 1);

    layout->addWidget(header);

    // ── Input / Expected / Actual output section ──
    layout->addWidget(createSectionHeader(tr("输入输出对比")));

    if (!m_record.inputData.isEmpty())
        layout->addWidget(createOutputBlock(tr("输入:"), m_record.inputData));
    layout->addWidget(createOutputBlock(tr("期望输出:"), m_record.expectedOutput));
    layout->addWidget(createOutputBlock(tr("实际输出:"), m_record.actualOutput));

    // ── AI Analysis section ──
    layout->addWidget(createSectionHeader(tr("AI 分析")));

    m_aiAnalysisLabel = new QLabel;
    m_aiAnalysisLabel->setObjectName(QStringLiteral("detailAiAnalysis"));
    m_aiAnalysisLabel->setTextFormat(Qt::RichText);
    m_aiAnalysisLabel->setWordWrap(true);
    m_aiAnalysisLabel->setOpenExternalLinks(true);
    m_aiAnalysisLabel->setMinimumHeight(40);

    if (!m_record.aiAnalysis.isEmpty()) {
        QString html = renderMarkdown(m_record.aiAnalysis);
        m_aiAnalysisLabel->setText(html);
    } else {
        m_aiAnalysisLabel->setText(QStringLiteral(
            "<span>"
            "尚未进行 AI 分析，点击下方「重新分析」按钮开始分析。</span>"));
    }

    layout->addWidget(m_aiAnalysisLabel);

    // ── Action buttons ──
    auto *btnBar = new QWidget;
    btnBar->setObjectName(QStringLiteral("detailBtnBar"));
    auto *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(12, 6, 12, 6);
    btnLayout->setSpacing(8);

    m_reanalyzeBtn = new QPushButton(tr("🔄 分析"));
    m_reanalyzeBtn->setFixedHeight(26);
    m_reanalyzeBtn->setCursor(Qt::PointingHandCursor);

    m_deleteBtn = new QPushButton(tr("🗑 删除"));
    m_deleteBtn->setFixedHeight(26);
    m_deleteBtn->setCursor(Qt::PointingHandCursor);

    m_reviewBtn = new QPushButton(m_record.reviewed ? tr("✅ 已阅") : tr("标记已阅"));
    m_reviewBtn->setFixedHeight(26);
    m_reviewBtn->setCursor(Qt::PointingHandCursor);

    btnLayout->addWidget(m_reanalyzeBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addWidget(m_reviewBtn);
    btnLayout->addStretch();

    layout->addWidget(btnBar);

    // ── Connections ──
    connect(m_reanalyzeBtn, &QPushButton::clicked, this, [this]() {
        emit reanalyzeClicked(m_record.id);
    });
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        emit deleteClicked(m_record.id);
    });
    connect(m_reviewBtn, &QPushButton::clicked, this, [this]() {
        m_record.reviewed = !m_record.reviewed;
        m_reviewBtn->setText(m_record.reviewed ? tr("✅ 已阅") : tr("标记已阅"));
        emit markReviewed(m_record.id, m_record.reviewed);
    });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() { refreshStyles(); });
    refreshStyles();
}

void ErrorDetailWidget::refreshStyles()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QStringLiteral(
        "background-color: %1;"
        "#detailHeader { background-color: %2; }"
        "#detailBtnBar { background-color: %2; }"
        "#detailStatusLbl { color: %3; font-size: 12px; }"
        "#detailTimeLbl, #detailMemLbl { color: %4; font-size: 11px; }"
        "#detailFileLbl { color: %5; font-size: 11px; }"
    ).arg(tm.color("editorLineNumber.background").name(),      // #252525
          tm.color("list.hoverBackground").name(),              // #2A2D2E (replaces #2a2a2a)
          tm.color("workbench.foreground").name(),              // #CCCCCC (replaces #ccc)
          tm.color("editorLineNumber.foreground").name(),       // #858585 (replaces #aaa/#888)
          tm.color("tab.inactiveForeground").name()));          // #969696 (replaces #888)

    m_aiAnalysisLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 12px; padding: 8px 12px;"
        "background-color: %2;"
    ).arg(tm.color("workbench.foreground").name(),
          tm.color("editor.background").name()));

    // Buttons
    m_reanalyzeBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: %1; color: %2; border: none;"
        "  border-radius: 4px; font-size: 11px; padding: 0 12px;"
        "}"
        "QPushButton:hover { background-color: %3; }"
    ).arg(tm.color("button.background").name(),
          tm.color("button.foreground").name(),
          tm.color("button.hoverBackground").name()));

    m_deleteBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: transparent; color: %1;"
        "  border: 1px solid %2; border-radius: 4px;"
        "  font-size: 11px; padding: 0 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3; border-color: %1;"
        "}"
    ).arg(tm.color("diagnostics.error").name(),
          tm.color("input.border").name(),
          tm.color("cell.badge.error").name()));

    m_reviewBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: transparent; color: %1;"
        "  border: 1px solid %2; border-radius: 4px;"
        "  font-size: 11px; padding: 0 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3; border-color: %1;"
        "}"
    ).arg(tm.color("syntax.types").name(),      // #4EC9B0
          tm.color("input.border").name(),
          QStringLiteral("#1E3C3C")));
}

void ErrorDetailWidget::setAnalysis(const QString &analysis)
{
    m_record.aiAnalysis = analysis;
    if (analysis.isEmpty()) {
        m_aiAnalysisLabel->setText(QStringLiteral(
            "<span>正在分析中...</span>"));
        return;
    }

    QString html = renderMarkdown(analysis);
    m_aiAnalysisLabel->setText(html);
}

void ErrorDetailWidget::setReviewed(bool reviewed)
{
    m_record.reviewed = reviewed;
    m_reviewBtn->setText(reviewed ? tr("✅ 已阅") : tr("标记已阅"));
}

// ═══════════════════════════════════════════════════════════════════
// ErrorListPanel — filter/search bar + scrollable list + bottom bar
// ═══════════════════════════════════════════════════════════════════

ErrorListPanel::ErrorListPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Filter bar ──
    m_filterBar = new QWidget(this);
    m_filterBar->setObjectName(QStringLiteral("errorFilterBar"));
    auto *filterLayout = new QVBoxLayout(m_filterBar);
    filterLayout->setContentsMargins(8, 6, 8, 6);
    filterLayout->setSpacing(6);

    m_statusFilter = new QComboBox;
    m_statusFilter->addItem(tr("全部状态"), QString());
    m_statusFilter->addItem("WA", "WA");
    m_statusFilter->addItem("RE", "RE");
    m_statusFilter->addItem("TLE", "TLE");
    m_statusFilter->addItem("MLE", "MLE");

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("搜索错题..."));

    filterLayout->addWidget(m_statusFilter);
    filterLayout->addWidget(m_searchEdit);

    // ── Scroll area for error list ──
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_listContainer = new QWidget;
    m_listLayout = new QVBoxLayout(m_listContainer);
    m_listLayout->setContentsMargins(4, 4, 4, 4);
    m_listLayout->setSpacing(2);
    m_listLayout->addStretch();

    m_scrollArea->setWidget(m_listContainer);

    // ── Bottom bar ──
    m_bottomBar = new QWidget(this);
    m_bottomBar->setFixedHeight(32);
    m_bottomBar->setObjectName(QStringLiteral("errorBottomBar"));
    auto *bottomLayout = new QHBoxLayout(m_bottomBar);
    bottomLayout->setContentsMargins(8, 0, 8, 0);

    m_deleteAllBtn = new QPushButton(tr("全部删除"));
    m_deleteAllBtn->setFixedHeight(22);
    m_deleteAllBtn->setCursor(Qt::PointingHandCursor);

    m_countLabel = new QLabel(tr("共 0 条记录"));

    bottomLayout->addWidget(m_deleteAllBtn);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_countLabel);

    // ── Assemble main layout ──
    mainLayout->addWidget(m_filterBar);
    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(m_bottomBar);

    // ── Connections ──
    connect(m_statusFilter, &QComboBox::currentIndexChanged,
            this, &ErrorListPanel::onFilterChanged);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &ErrorListPanel::onSearchTextChanged);
    connect(m_deleteAllBtn, &QPushButton::clicked,
            this, &ErrorListPanel::deleteAllRequested);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ErrorListPanel::refreshStyle);
    refreshStyle();
}

void ErrorListPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QStringLiteral(
        "background-color: %1;"
        "#errorFilterBar { background-color: %2; }"
        "#errorBottomBar { background-color: %2; }"
    ).arg(tm.color("editor.background").name(),
          tm.color("editorLineNumber.background").name()));

    m_statusFilter->setStyleSheet(QStringLiteral(
        "QComboBox {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-size: 12px;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 20px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: none;"
        "  border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent;"
        "  border-top: 6px solid %4;"
        "  margin-right: 4px;"
        "}"
        "QComboBox:hover {"
        "  border-color: %5;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: %6;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  selection-background-color: %7;"
        "  font-size: 12px;"
        "}"
    ).arg(tm.color("dropdown.background").name(),
          tm.color("menu.foreground").name(),
          tm.color("dropdown.border").name(),
          tm.color("editorLineNumber.foreground").name(),
          tm.color("activityBar.activeBorder").name(),
          tm.color("menu.background").name(),
          tm.color("menu.selectionBackground").name()));

    m_searchEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: %4;"
        "}"
        "QLineEdit::placeholder {"
        "  color: %5;"
        "}"
    ).arg(tm.color("input.background").name(),
          tm.color("input.foreground").name(),
          tm.color("input.border").name(),
          tm.color("activityBar.activeBorder").name(),
          tm.color("editorLineNumber.foreground").name()));

    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background-color: %1; border: none; }"
        "QScrollBar:vertical {"
        "  background-color: %1;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: %2;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    ).arg(tm.color("editor.background").name(),
          tm.color("scrollbarSlider.background").name()));

    m_listContainer->setStyleSheet(QStringLiteral(
        "background-color: %1;"
    ).arg(tm.color("editor.background").name()));

    m_deleteAllBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  border: 1px solid %1;"
        "  border-radius: 4px;"
        "  color: %2;"
        "  font-size: 11px;"
        "  padding: 0 8px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3;"
        "  border-color: %2;"
        "}"
    ).arg(tm.color("input.border").name(),
          tm.color("diagnostics.error").name(),
          tm.color("cell.badge.error").name()));

    m_countLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px;"
    ).arg(tm.color("editorLineNumber.foreground").name()));
}

// ── Data loading ─────────────────────────────────────────────────

void ErrorListPanel::loadRecords()
{
    setRecords(ErrorJournal::instance().allRecords());
}

void ErrorListPanel::setRecords(const QVector<ErrorRecord> &records)
{
    m_allRecords = records;
    m_expandedId.clear();
    // Sort by timestamp descending
    std::sort(m_allRecords.begin(), m_allRecords.end(),
              [](const ErrorRecord &a, const ErrorRecord &b) {
                  return a.timestamp > b.timestamp;
              });
    rebuildList();
}

// ── Filtering ────────────────────────────────────────────────────

void ErrorListPanel::onFilterChanged()
{
    applyFilter();
}

void ErrorListPanel::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    applyFilter();
}

void ErrorListPanel::rebuildList()
{
    // Remove all existing item and detail widgets
    qDeleteAll(m_listItems);
    m_listItems.clear();
    qDeleteAll(m_detailWidgets);
    m_detailWidgets.clear();

    QVector<ErrorRecord> filtered = filteredRecords();
    m_countLabel->setText(tr("共 %1 条记录").arg(filtered.size()));

    for (const auto &record : filtered) {
        auto *item = new ErrorListItem(record, m_listContainer);
        connect(item, &ErrorListItem::clicked, this, &ErrorListPanel::onItemClicked);
        m_listLayout->insertWidget(m_listLayout->count() - 1, item);
        m_listItems.append(item);

        if (m_expandedId == record.id) {
            auto *detail = createDetailWidget(record);
            m_listLayout->insertWidget(m_listLayout->count() - 1, detail);
        }
    }

    m_scrollArea->verticalScrollBar()->setValue(0);
}

void ErrorListPanel::applyFilter()
{
    QVector<ErrorRecord> filtered = filteredRecords();
    m_countLabel->setText(tr("共 %1 条记录").arg(filtered.size()));

    QSet<QString> filteredIds;
    for (const auto &rec : filtered)
        filteredIds.insert(rec.id);

    for (auto *item : m_listItems)
        item->setVisible(filteredIds.contains(item->recordId()));

    // Hide all detail widgets and clear expanded state
    for (auto *detail : m_detailWidgets)
        detail->hide();
    m_expandedId.clear();

    m_scrollArea->verticalScrollBar()->setValue(0);
}

QVector<ErrorRecord> ErrorListPanel::filteredRecords() const
{
    QString statusFilter = m_statusFilter->currentData().toString();
    QString keyword = m_searchEdit->text().trimmed().toLower();

    QVector<ErrorRecord> result;
    for (const auto &rec : m_allRecords) {
        if (!statusFilter.isEmpty() && rec.statusCode != statusFilter)
            continue;

        if (!keyword.isEmpty()) {
            bool match = rec.problemName.toLower().contains(keyword)
                      || rec.sourceFile.toLower().contains(keyword)
                      || rec.statusCode.toLower().contains(keyword);
            if (!match) {
                for (const auto &tag : rec.tags) {
                    if (tag.toLower().contains(keyword)) {
                        match = true;
                        break;
                    }
                }
            }
            if (!match)
                continue;
        }

        result.append(rec);
    }

    return result;
}

// ── Item expand / collapse ───────────────────────────────────────

void ErrorListPanel::onItemClicked(const QString &recordId)
{
    if (m_expandedId == recordId) {
        collapseItem(recordId);
    } else {
        expandItem(recordId);
    }
}

void ErrorListPanel::expandItem(const QString &recordId)
{
    m_expandedId = recordId;

    // Hide any other expanded detail
    for (auto it = m_detailWidgets.begin(); it != m_detailWidgets.end(); ++it) {
        if (it.key() != recordId)
            it.value()->hide();
    }

    // If detail already exists, just show it
    if (m_detailWidgets.contains(recordId)) {
        m_detailWidgets.value(recordId)->show();
        return;
    }

    // Find the record and create the detail widget
    ErrorRecord rec = ErrorJournal::instance().recordById(recordId);
    if (rec.id.isEmpty())
        return;

    auto *detail = createDetailWidget(rec);
    for (auto *item : m_listItems) {
        if (item->recordId() == recordId) {
            int idx = m_listLayout->indexOf(item);
            m_listLayout->insertWidget(idx + 1, detail);
            break;
        }
    }
}

void ErrorListPanel::collapseItem(const QString &recordId)
{
    Q_UNUSED(recordId);
    m_expandedId.clear();
    for (auto *detail : m_detailWidgets)
        detail->hide();
}

ErrorDetailWidget *ErrorListPanel::findDetail(const QString &recordId) const
{
    return m_detailWidgets.value(recordId, nullptr);
}

ErrorDetailWidget *ErrorListPanel::createDetailWidget(const ErrorRecord &record)
{
    auto *detail = new ErrorDetailWidget(record, m_listContainer);
    connect(detail, &ErrorDetailWidget::reanalyzeClicked,
            this, &ErrorListPanel::onReanalyzeClicked);
    connect(detail, &ErrorDetailWidget::deleteClicked,
            this, &ErrorListPanel::deleteRecordRequested);
    connect(detail, &ErrorDetailWidget::markReviewed,
            this, [](const QString &recId, bool reviewed) {
        ErrorJournal::instance().setRecordReviewed(recId, reviewed);
    });
    m_detailWidgets.insert(record.id, detail);
    return detail;
}

void ErrorListPanel::onReanalyzeClicked(const QString &recordId)
{
    // Update detail widget to show "analyzing..."
    ErrorDetailWidget *detail = findDetail(recordId);
    if (detail)
        detail->setAnalysis(QString());

    emit reanalyzeRequested(recordId);
    ErrorJournal::instance().requestAnalysis(recordId);
}

void ErrorListPanel::updateAnalysis(const QString &recordId)
{
    ErrorDetailWidget *detail = findDetail(recordId);
    if (!detail)
        return;

    ErrorRecord rec = ErrorJournal::instance().recordById(recordId);
    detail->setAnalysis(rec.aiAnalysis);
}
