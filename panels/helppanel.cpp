#include "helppanel.h"
#include "thememanager.h"
#include "windowdraghelper.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QScrollBar>
#include <QMouseEvent>
#include <QShowEvent>
#include <QFile>
#include <QApplication>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>

HelpPanel::HelpPanel(QWidget *parent)
    : QWidget(parent)
{
    resize(880, 620);
    setObjectName("helpPanel");
    setMinimumSize(400, 300);

    setAutoFillBackground(true);

    // ---- section info ----
    m_sectionInfo = {
        {"welcome",          tr("欢迎使用"),    QStringLiteral("Smart Markdown — 帮助文档")},
        {"file-management",  tr("文件管理"),    QStringLiteral("文件管理")},
        {"editor",           tr("编辑器"),      QStringLiteral("编辑器")},
        {"markdown-preview",  tr("Markdown 预览"), QStringLiteral("Markdown 预览")},
        {"smd-editor",       tr("SMD 编辑器"),  QStringLiteral("SMD 编辑器")},
        {"compile-run",      tr("代码编译运行"), QStringLiteral("代码编译运行")},
        {"search-outline",   tr("搜索与大纲"),  QStringLiteral("搜索与大纲")},
        {"wikilinks-tags",   tr("双向链接与标签"), QStringLiteral("双向链接与标签")},
        {"history-backlinks",tr("历史记录"),    QStringLiteral("历史记录")},
        {"judge",            tr("评测系统"),    QStringLiteral("评测系统")},
        {"openjudge",        tr("OpenJudge 集成"), QStringLiteral("OpenJudge 集成")},
        {"ai-assistant",     tr("AI 助手"),    QStringLiteral("AI 助手")},
        {"settings",         tr("设置与快捷键"),QStringLiteral("设置与快捷键")},
        {"shortcuts",        tr("快捷键参考"),  QStringLiteral("快捷键参考")},
    };

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- Title bar ----
    auto *titleBar = new QWidget;
    titleBar->setFixedHeight(kTitleBarHeight);
    titleBar->setObjectName("helpTitleBar");

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 8, 0);

    m_titleLabel = new QLabel(tr("帮助"));

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();

    m_closeButton = new QPushButton(QStringLiteral("✕"));
    m_closeButton->setFixedSize(28, 24);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    connect(m_closeButton, &QPushButton::clicked, this, &HelpPanel::closeRequested);

    titleLayout->addWidget(m_closeButton);
    mainLayout->addWidget(titleBar);

    // ---- Body ----
    auto *bodyLayout = new QHBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // Left category list
    m_categoryList = new QListWidget;
    m_categoryList->setFixedWidth(kCategoryWidth);

    for (const auto &info : m_sectionInfo) {
        auto *item = new QListWidgetItem(info.displayText);
        item->setData(Qt::UserRole, info.id);
        m_categoryList->addItem(item);
    }

    bodyLayout->addWidget(m_categoryList);

    // Right content
    m_contentBrowser = new QTextBrowser;
    m_contentBrowser->setOpenExternalLinks(true);

    bodyLayout->addWidget(m_contentBrowser, 1);
    mainLayout->addLayout(bodyLayout, 1);

    // No size grip — panel is not user-resizable

    // Category list selection → scroll to section
    connect(m_categoryList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_updatingCategory) return;
        if (row < 0) return;
        auto *item = m_categoryList->item(row);
        QString anchor = item->data(Qt::UserRole).toString();
        m_contentBrowser->scrollToAnchor(anchor);
    });

    // Load content
    loadContent();

    // Connect scrollbar for content → category sync
    m_scrollBar = m_contentBrowser->verticalScrollBar();
    connect(m_scrollBar, &QScrollBar::valueChanged, this, &HelpPanel::onScrollChanged);

    // Select first category
    m_categoryList->setCurrentRow(0);

    ThemeManager::watchTheme(this, &HelpPanel::refreshStyle);
    refreshStyle();
}

void HelpPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, tm.color("menu.background"));
    setPalette(pal);

    setStyleSheet(QStringLiteral(
        "#helpPanel {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}")
        .arg(tm.color("menu.background").name(),
             tm.color("panel.border").name()));

    if (auto *titleBar = findChild<QWidget*>("helpTitleBar")) {
        titleBar->setStyleSheet(QStringLiteral(
            "#helpTitleBar {"
            "  background-color: %1;"
            "  border-top-left-radius: 8px;"
            "  border-top-right-radius: 8px;"
            "}")
            .arg(tm.color("activityBar.background").name()));
    }

    m_titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: bold;")
                                .arg(tm.color("titleBar.foreground").name()));

    m_closeButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; }"
        "QPushButton:hover { color: %2; background: %3; border-radius: 3px; }")
        .arg(tm.color("tab.inactiveForeground").name(),
             tm.color("badge.foreground").name(),
             tm.color("titleBar.buttonCloseHover").name()));

    m_categoryList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background-color: %1;"
        "  border: none;"
        "  border-right: 1px solid %2;"
        "  color: %3;"
        "  font-size: 13px;"
        "  outline: none;"
        "}"
        "QListWidget::item { padding: 8px 12px; border: none; }"
        "QListWidget::item:selected { background-color: %4; color: %5; }"
        "QListWidget::item:hover:!selected { background-color: %6; }")
        .arg(tm.color("editorLineNumber.background").name(),
             tm.color("panel.border").name(),
             tm.color("menu.foreground").name(),
             tm.color("list.activeBackground").name(),
             tm.color("badge.foreground").name(),
             tm.color("list.hoverBackground").name()));

    m_contentBrowser->setStyleSheet(QStringLiteral(
        "QTextBrowser { background-color: %1; color: %2; border: none; font-size: 14px; padding: 8px; }")
        .arg(tm.color("editor.background").name(),
             tm.color("editor.foreground").name()));

    // Re-theme the HTML content with current theme colors
    applyThemeToContent();
}

void HelpPanel::applyThemeToContent()
{
    auto &tm = ThemeManager::instance();
    if (m_rawHelpContent.isEmpty())
        return;

    QString bg = tm.color("editor.background").name();
    QString fg = tm.color("editor.foreground").name();
    QString heading1 = tm.color("workbench.foreground").name();
    QString heading2 = tm.color("syntax.keywords").name();
    QString heading3 = tm.color("syntax.types").name();
    QString codeBg = tm.color("aiAssistant.codeBackground").name();
    QString codeFg = tm.color("aiAssistant.codeForeground").name();
    QString border = tm.color("panel.border").name();
    QString subtle = tm.color("editorLineNumber.foreground").name();
    QString noteBg = tm.color("list.hoverBackground").name();

    QString html = m_rawHelpContent;

    // Replace hardcoded dark theme colors — longer matches first to avoid substring issues
    html.replace(QRegularExpression("#1e1e1e\\b"), bg);
    html.replace(QRegularExpression("#252526\\b"), codeBg);
    html.replace(QRegularExpression("#2d2d2d\\b"), noteBg);
    html.replace(QRegularExpression("#404040\\b"), border);
    html.replace(QRegularExpression("#d4d4d4\\b"), fg);
    html.replace(QRegularExpression("#e0e0e0\\b"), heading1);
    html.replace(QRegularExpression("#569cd6\\b"), heading2);
    html.replace(QRegularExpression("#4ec9b0\\b"), heading3);
    html.replace(QRegularExpression("#ce9178\\b"), codeFg);
    html.replace(QRegularExpression("#b0b0b0\\b"), subtle);
    // 3-digit hex: replace after longer patterns to avoid substring conflicts
    html.replace(QRegularExpression("#333\\b"), border);
    html.replace(QRegularExpression("#555\\b"), border);

    m_contentBrowser->setHtml(html);
}

void HelpPanel::loadContent()
{
    QFile file(":/help/content");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_rawHelpContent = QString::fromUtf8(file.readAll());
        applyThemeToContent();
    } else {
        m_contentBrowser->setPlainText(tr("无法加载帮助内容"));
    }
}

void HelpPanel::computeSectionPositions()
{
    auto *doc = m_contentBrowser->document();
    auto *layout = doc->documentLayout();
    if (!layout) return;

    m_sectionPositions.resize(m_sectionInfo.size());
    std::fill(m_sectionPositions.begin(), m_sectionPositions.end(), -1);

    QTextCursor cursor(doc);
    for (int i = 0; i < m_sectionInfo.size(); ++i) {
        QTextCursor found = doc->find(m_sectionInfo[i].matchText, cursor);
        if (!found.isNull()) {
            QTextBlock block = found.block();
            qreal y = layout->blockBoundingRect(block).y();
            m_sectionPositions[i] = y;
            cursor = found;
        }
    }
}

void HelpPanel::onScrollChanged(int value)
{
    if (m_sectionPositions.isEmpty() || m_sectionPositions.last() < 0)
        return;

    // value is pixel scroll position — compare against section Y positions
    int bestIndex = 0;
    for (int i = 1; i < m_sectionPositions.size(); ++i) {
        if (m_sectionPositions[i] >= 0 && m_sectionPositions[i] <= value + 2) {
            bestIndex = i;
        }
    }

    if (m_categoryList->currentRow() != bestIndex) {
        m_updatingCategory = true;
        m_categoryList->setCurrentRow(bestIndex);
        m_updatingCategory = false;
    }
}

void HelpPanel::mousePressEvent(QMouseEvent *event)
{
    m_dragHelper.handlePress(this, event, kTitleBarHeight);
    QWidget::mousePressEvent(event);
}

void HelpPanel::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragHelper.handleMove(this, event))
        QWidget::mouseMoveEvent(event);
}

void HelpPanel::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragHelper.handleRelease(event);
    QWidget::mouseReleaseEvent(event);
}

void HelpPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_positionsComputed) {
        computeSectionPositions();
        m_positionsComputed = true;
    }
}
