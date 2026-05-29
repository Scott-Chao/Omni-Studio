#include "openjudgewidget.h"
#include "codeeditor.h"
#include "logindialog.h"
#include "settingsmanager.h"
#include "configmanager.h"
#include "thememanager.h"

#include <QComboBox>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QStackedWidget>
#include <QListWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QFont>
#include <QFrame>
#include <QTimer>
#include <QUrlQuery>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QStandardPaths>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QMouseEvent>

// ===== HomeworkDelegate: draws deadline right-aligned =====
class HomeworkDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        opt.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(painter, opt, index);

        QString deadline = index.data(Qt::UserRole + 1).toString();
        if (deadline.isEmpty())
            return;

        painter->save();
        painter->setPen(ThemeManager::instance().color("tab.inactiveForeground"));
        QFont f = painter->font();
        f.setPointSize(qMax(f.pointSize() - 1, 8));
        painter->setFont(f);

        QRect r = option.rect.adjusted(0, 0, -10, 0);
        QString elided = painter->fontMetrics().elidedText(deadline, Qt::ElideLeft, r.width() / 2);
        painter->drawText(r, Qt::AlignRight | Qt::AlignVCenter, elided);
        painter->restore();
    }
};

// ===== NoFocusDelegate: prevents focus rect drawing =====
class NoFocusDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        opt.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

// ======================================================================
// Constructor
// ======================================================================

OpenJudgeWidget::OpenJudgeWidget(SettingsManager *settings, QWidget *parent)
    : QWidget(parent)
    , m_crawler(new Crawler(this))
    , m_listWidget(new QListWidget)
    , m_statusLabel(new QLabel)
    , m_refreshBtn(new QPushButton(QStringLiteral("刷新")))
    , m_backBtn(new QPushButton(QStringLiteral("← 返回")))
    , m_selectBtn(new QPushButton(QStringLiteral("选择此题目")))
    , m_stackedWidget(new QStackedWidget)
    , m_detailPage(new QWidget)
    , m_sectionList(new QListWidget)
    , m_sectionContent(new QTextBrowser)
    , m_settingsManager(settings)
{
    setupUi();

    // --- Crawler signals ---
    connect(m_crawler, &Crawler::loginSuccess, this, &OpenJudgeWidget::onLoginSuccess);
    connect(m_crawler, &Crawler::loginFailed, this, &OpenJudgeWidget::onLoginFailed);
    connect(m_crawler, &Crawler::mainPageReady, this, &OpenJudgeWidget::onMainPageReady);
    connect(m_crawler, &Crawler::pastPageReady, this, &OpenJudgeWidget::onPastPageReady);
    connect(m_crawler, &Crawler::homeworkProblemsReady, this, &OpenJudgeWidget::onHomeworkProblemsReady);
    connect(m_crawler, &Crawler::problemDetailReady, this, &OpenJudgeWidget::onProblemDetailReady);
    connect(m_crawler, &Crawler::networkError, this, &OpenJudgeWidget::onNetworkError);

    // --- UI signals ---
    connect(m_listWidget, &QListWidget::itemClicked, this, &OpenJudgeWidget::onItemClicked);
    connect(m_sectionList, &QListWidget::itemClicked, this, &OpenJudgeWidget::onSectionClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &OpenJudgeWidget::onRefresh);
    connect(m_backBtn, &QPushButton::clicked, this, &OpenJudgeWidget::onBack);
    connect(m_prevPageBtn, &QPushButton::clicked, this, &OpenJudgeWidget::onPrevPage);
    connect(m_nextPageBtn, &QPushButton::clicked, this, &OpenJudgeWidget::onNextPage);
    connect(m_selectBtn, &QPushButton::clicked, this, &OpenJudgeWidget::onSelectClicked);

    // --- Crawler submission signals (forwarded) ---
    connect(m_crawler, &Crawler::submissionResultReady,
            this, &OpenJudgeWidget::submissionResultReady);
    connect(m_crawler, &Crawler::submissionFailed, this, [this](const QString &error) {
        m_refreshBtn->setEnabled(true);
        m_statusLabel->setText(QStringLiteral("提交失败"));
        emit submissionFailed(error);
    });
    connect(m_crawler, &Crawler::submitPollTimeout, this, [this]() {
        m_statusLabel->setText(QStringLiteral("提交结果获取超时"));
    });

    // Login dialog is triggered by MainWindow::onOpenJudgeRequested after widget is shown

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &OpenJudgeWidget::refreshStyle);

    m_listWidget->installEventFilter(this);
}

// ======================================================================
// UI Construction
// ======================================================================

static QString buttonStyle(const QColor &bg, const QColor &fg, const QColor &border,
                           const QColor &hover, const QColor &disabledFg)
{
    return QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 3px; padding: 4px 12px; } "
        "QPushButton:hover { background: %4; } "
        "QPushButton:disabled { color: %5; }")
        .arg(bg.name(), fg.name(), border.name(), hover.name(), disabledFg.name());
}

void OpenJudgeWidget::setupUi()
{
    auto &tm = ThemeManager::instance();
    auto btnBg = tm.color("input.background");
    auto btnFg = tm.color("input.foreground");
    auto btnBorder = tm.color("input.border");
    auto btnHover = tm.color("aiAssistant.actionButtonHoverBackground");
    auto disabledFg = tm.color("tab.inactiveForeground");

    setStyleSheet(QStringLiteral(
        "OpenJudgeWidget { background: %1; }"
        "QWidget { background: %1; color: %2; }")
        .arg(tm.color("menu.background").name(),
             tm.color("editor.foreground").name()));

    // --- Top toolbar ---
    m_toolbar = new QWidget;
    m_toolbar->setStyleSheet(QStringLiteral("QWidget { background: %1; }")
                           .arg(tm.color("activityBar.background").name()));
    auto *toolbarLayout = new QHBoxLayout(m_toolbar);
    toolbarLayout->setContentsMargins(12, 8, 12, 8);

    m_statusLabel->setVisible(false);

    // Select button — wide, prominent, in toolbar
    m_selectBtn->setMinimumWidth(ConfigManager::instance().openJudgeSelectButtonMinWidth());
    m_selectBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; color: white; font-weight: bold; "
        "border: none; border-radius: 4px; padding: 6px 20px; } "
        "QPushButton:hover { background: %2; } "
        "QPushButton:disabled { background: %3; color: %4; }")
        .arg(tm.color("badge.background").name(),
             tm.color("badge.background").lighter(115).name(),
             btnBg.name(), disabledFg.name()));
    m_selectBtn->setVisible(false);

    m_toggleSidebarBtn = new QPushButton(QStringLiteral("显示栏目"));
    m_toggleSidebarBtn->setStyleSheet(buttonStyle(btnBg, btnFg, btnBorder, btnHover, disabledFg));
    m_toggleSidebarBtn->setVisible(false);
    connect(m_toggleSidebarBtn, &QPushButton::clicked, this, &OpenJudgeWidget::onToggleSidebar);

    m_toggleProblemBtn = new QPushButton(QStringLiteral("隐藏题目"));
    m_toggleProblemBtn->setStyleSheet(buttonStyle(btnBg, btnFg, btnBorder, btnHover, disabledFg));
    m_toggleProblemBtn->setVisible(false);
    connect(m_toggleProblemBtn, &QPushButton::clicked, this, &OpenJudgeWidget::onToggleProblem);

    m_ideBtn = new QPushButton(QStringLiteral("IDE"));
    m_ideBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 3px; padding: 4px 12px; } "
        "QPushButton:hover { background: %4; } "
        "QPushButton:checked { background: %5; color: white; font-weight: bold; } "
        "QPushButton:disabled { color: %6; }")
        .arg(btnBg.name(), btnFg.name(), btnBorder.name(), btnHover.name(),
             tm.color("editor.selectionBackground").name(), disabledFg.name()));
    m_ideBtn->setCheckable(true);
    m_ideBtn->setVisible(false);
    connect(m_ideBtn, &QPushButton::clicked, this, &OpenJudgeWidget::onToggleIdeMode);

    m_langCombo = new QComboBox;
    m_langCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 2px; padding: 2px 6px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: %1; color: %2; "
        "selection-background-color: %4; }")
        .arg(btnBg.name(), btnFg.name(), btnBorder.name(),
             tm.color("editor.selectionBackground").name()));
    m_langCombo->setVisible(false);
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenJudgeWidget::onIdeLanguageChanged);

    m_refreshBtn->setStyleSheet(buttonStyle(btnBg, btnFg, btnBorder, btnHover, disabledFg));
    m_refreshBtn->setEnabled(false);
    m_backBtn->setStyleSheet(buttonStyle(btnBg, btnFg, btnBorder, btnHover, disabledFg));
    m_backBtn->setVisible(false);

    m_userLabel = new QLabel;
    m_userLabel->setVisible(false);
    m_userLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; padding: 0 8px;")
                               .arg(tm.color("judge.ac").name()));

    m_loginBtn = new QPushButton(QStringLiteral("登录"));
    m_loginBtn->setStyleSheet(buttonStyle(btnBg, btnFg, btnBorder, btnHover, disabledFg));
    connect(m_loginBtn, &QPushButton::clicked, this, &OpenJudgeWidget::onLoginLogoutClicked);

    toolbarLayout->addWidget(m_selectBtn);
    toolbarLayout->addWidget(m_toggleSidebarBtn);
    toolbarLayout->addWidget(m_toggleProblemBtn);
    toolbarLayout->addWidget(m_ideBtn);
    toolbarLayout->addWidget(m_langCombo);
    toolbarLayout->addWidget(m_backBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_refreshBtn);
    toolbarLayout->addWidget(m_userLabel);
    toolbarLayout->addWidget(m_loginBtn);

    // --- Separator ---
    m_separator = new QFrame;
    m_separator->setFrameShape(QFrame::HLine);
    m_separator->setFrameShadow(QFrame::Sunken);
    m_separator->setStyleSheet(QStringLiteral("QFrame { color: %1; }").arg(tm.color("input.border").name()));

    // --- Setup stacked widget pages ---
    setupDetailPage();

    // Page 0: list + pagination
    {
        auto *listPage = new QWidget;
        auto *listLayout = new QVBoxLayout(listPage);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->addWidget(m_listWidget, 1);

        m_paginationBar = new QWidget;
        auto *paginationLayout = new QHBoxLayout(m_paginationBar);
        paginationLayout->setContentsMargins(8, 4, 8, 4);
        m_prevPageBtn = new QPushButton(QStringLiteral("上一页"));
        m_nextPageBtn = new QPushButton(QStringLiteral("下一页"));
        m_pageLabel = new QLabel(QStringLiteral("第 1 页"));
        m_pageLabel->setStyleSheet(QStringLiteral("color: %1;").arg(tm.color("editor.foreground").name()));
        m_pageLabel->setAlignment(Qt::AlignCenter);
        m_prevPageBtn->setStyleSheet(buttonStyle(btnBg, btnFg, btnBorder, btnHover, disabledFg));
        m_nextPageBtn->setStyleSheet(buttonStyle(btnBg, btnFg, btnBorder, btnHover, disabledFg));
        paginationLayout->addStretch();
        paginationLayout->addWidget(m_prevPageBtn);
        paginationLayout->addWidget(m_pageLabel);
        paginationLayout->addWidget(m_nextPageBtn);
        paginationLayout->addStretch();
        m_paginationBar->setVisible(false);

        listLayout->addWidget(m_paginationBar);
        m_stackedWidget->addWidget(listPage);         // page 0: list + pagination
    }
    m_stackedWidget->addWidget(m_detailPage);          // page 1: problem detail

    // --- Main layout (directly on this widget, no centralWidget) ---
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_toolbar);
    mainLayout->addWidget(m_separator);
    mainLayout->addWidget(m_stackedWidget, 1);

    // --- Style the list ---
    m_listWidget->setFont(QFont("Microsoft YaHei", 10));
    m_listWidget->setSpacing(4);
    m_listWidget->setCursor(Qt::PointingHandCursor);
    m_listWidget->setAlternatingRowColors(false);
    m_listWidget->setItemDelegate(new HomeworkDelegate(m_listWidget));
    m_listWidget->setStyleSheet(QStringLiteral(
        "QListWidget { color: %1; background: %2; border: none; }"
        "QListWidget::item { padding: 6px 12px; outline: none; }"
        "QListWidget::item:hover { background: %3; }"
        "QListWidget::item:selected { background: %4; color: white; }")
        .arg(tm.color("editor.foreground").name(),
             tm.color("sideBar.background").name(),
             tm.color("list.hoverBackground").name(),
             tm.color("editor.selectionBackground").name()));
}

void OpenJudgeWidget::setupDetailPage()
{
    auto &tm = ThemeManager::instance();
    auto *layout = new QHBoxLayout(m_detailPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Left: section list — tight fit to content
    m_sectionList->setFont(QFont("Microsoft YaHei", 10));
    m_sectionList->setFixedWidth(ConfigManager::instance().openJudgeSectionListWidth());
    m_sectionList->setSpacing(2);
    m_sectionList->setItemDelegate(new NoFocusDelegate(m_sectionList));
    m_sectionList->setCursor(Qt::PointingHandCursor);
    m_sectionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sectionList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_sectionList->setStyleSheet(QStringLiteral(
        "QListWidget { color: %1; background: %2; border: none; }"
        "QListWidget::item { padding: 6px 8px; outline: none; }"
        "QListWidget::item:hover { background: %3; }"
        "QListWidget::item:selected { background: %4; color: white; }")
        .arg(tm.color("editor.foreground").name(),
             tm.color("sideBar.background").name(),
             tm.color("list.hoverBackground").name(),
             tm.color("editor.selectionBackground").name()));

    // Right: content browser
    m_sectionContent->setFont(QFont("Microsoft YaHei", 10));
    m_sectionContent->setOpenExternalLinks(true);
    m_sectionContent->setStyleSheet(QStringLiteral(
        "QTextBrowser { padding: 16px; background: %1; border: none; color: %2; }")
        .arg(tm.color("editor.background").name(),
             tm.color("editor.foreground").name()));
    m_sectionContent->document()->setDefaultStyleSheet(QStringLiteral(
        "pre { background-color: %1; padding: 12px; "
        "border: 1px solid %2; font-family: Consolas, monospace; color: %3; }"
        "body { color: %3; }")
        .arg(tm.color("menu.background").name(),
             tm.color("input.border").name(),
             tm.color("editor.foreground").name()));

    // IDE splitter: always present. Left = problem, Right = editor (created lazily)
    m_ideSplitter = new QSplitter(Qt::Horizontal);
    m_ideSplitter->setChildrenCollapsible(false);
    m_ideSplitter->addWidget(m_sectionContent);

    layout->addWidget(m_sectionList);
    layout->addWidget(m_ideSplitter, 1);

    connect(m_sectionContent->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &OpenJudgeWidget::onContentScrolled);
}

// ======================================================================
// Combined HTML builder (all sections in one document)
// ======================================================================

QString OpenJudgeWidget::buildCombinedHtml(const ProblemDetail &detail) const
{
    auto &tm = ThemeManager::instance();
    const QColor bg = tm.color("editor.background");
    const QColor fg = tm.color("editor.foreground");
    const QColor preBg = tm.color("menu.background");
    const QColor border = tm.color("input.border");
    const QColor link = tm.color("syntax.keywords");
    const QColor headingFg = tm.color("judge.ac");

    QString html;
    html += QStringLiteral(
        "<html><head><meta charset=\"utf-8\"><style>"
        "body{background:%1;color:%2;font-family:'Microsoft YaHei',sans-serif;font-size:10pt;}"
        "pre{background:%3;padding:12px;border:1px solid %4;font-family:Consolas,monospace;color:%2;}"
        "a{color:%5;}"
        "h2.oj-section-heading{color:%6;font-size:13pt;margin:16px 0 8px 0;padding-bottom:4px;border-bottom:2px solid %4;}"
        "hr.oj-section-divider{border:none;border-top:1px solid %4;margin:24px 0 12px 0;}"
        "</style></head><body><div class=\"oj-problem-content\">")
        .arg(bg.name(), fg.name(), preBg.name(), border.name(), link.name(), headingFg.name());

    for (int i = 0; i < detail.sections.size(); ++i) {
        const auto &sec = detail.sections[i];
        if (i > 0)
            html += QStringLiteral("<hr class=\"oj-section-divider\">");
        html += QStringLiteral("<a name=\"section-%1\"></a><h2 class=\"oj-section-heading\">%2</h2>")
                .arg(i).arg(sec.heading.toHtmlEscaped());
        html += sec.contentHtml;
    }

    html += QStringLiteral("</div></body></html>");
    return html;
}

// ======================================================================
// View switching
// ======================================================================

void OpenJudgeWidget::showListPage()
{
    // Exit IDE mode if active
    if (m_ideMode) {
        onToggleIdeMode();
    }
    m_stackedWidget->setCurrentIndex(0);
    m_selectBtn->setVisible(false);
    m_toggleSidebarBtn->setVisible(false);
    m_toggleProblemBtn->setVisible(false);
    m_ideBtn->setVisible(false);
    m_langCombo->setVisible(false);
    m_currentSectionIndex = -1;
}

void OpenJudgeWidget::showDetailPage(const ProblemDetail &detail)
{
    m_sectionList->clear();
    m_sectionContent->clear();
    m_sectionYOffsets.clear();

    for (const auto &sec : detail.sections) {
        auto *item = new QListWidgetItem(sec.heading);
        item->setSizeHint(QSize(0, 32));
        m_sectionList->addItem(item);
    }

    if (!detail.sections.isEmpty()) {
        m_sectionList->setCurrentRow(0);
        m_currentSectionIndex = 0;
        m_sectionContent->setHtml(buildCombinedHtml(detail));
        m_sectionContent->scrollToAnchor(QStringLiteral("section-0"));
        QTimer::singleShot(0, this, [this]() { recordSectionPositions(); });
    }

    // Exit IDE mode when switching to a new problem
    if (m_ideMode) {
        onToggleIdeMode();
    }

    // Sidebar hidden by default
    m_sidebarVisible = false;
    m_sectionList->setVisible(false);
    m_toggleSidebarBtn->setVisible(true);
    m_toggleSidebarBtn->setText(QStringLiteral("显示栏目"));

    m_ideBtn->setVisible(true);
    m_currentLangId = 1; // default to C++ (G++)
    m_stackedWidget->setCurrentIndex(1);
}

// ======================================================================
// Login / auth
// ======================================================================

bool OpenJudgeWidget::tryAutoLogin()
{
    if (m_isLoggedIn) return true;
    if (m_autoLoginInProgress) return false;
    if (!m_settingsManager) return false;
    if (!m_settingsManager->openJudgeAutoLogin()) return false;

    auto [username, password] = m_settingsManager->openJudgeCredentials();
    if (username.isEmpty() || password.isEmpty()) return false;

    m_autoLoginInProgress = true;
    m_username = username;
    m_statusLabel->setText(QStringLiteral("正在自动登录..."));
    m_refreshBtn->setEnabled(false);
    m_listWidget->clear();
    m_crawler->login(username, password);
    return true;
}

void OpenJudgeWidget::onReLogin()
{
    // Try auto-login first
    if (tryAutoLogin())
        return;

    LoginDialog dlg(this);
    if (m_settingsManager)
        dlg.setAutoLoginEnabled(m_settingsManager->openJudgeAutoLogin());

    if (dlg.exec() == QDialog::Accepted) {
        m_username = dlg.username();
        m_pendingPassword = dlg.password();
        m_pendingAutoLogin = dlg.isAutoLoginEnabled();
        m_statusLabel->setText(QStringLiteral("正在登录..."));
        m_refreshBtn->setEnabled(false);
        m_listWidget->clear();
        m_crawler->login(m_username, m_pendingPassword);
    } else {
        m_pendingAutoLogin = false;
        m_pendingPassword.clear();
        m_statusLabel->setText(QStringLiteral("跳过登录，正在获取页面..."));
        m_crawler->fetchMainPage();
    }
}

void OpenJudgeWidget::onLoginSuccess()
{
    m_isLoggedIn = true;
    m_autoLoginInProgress = false;
    m_loginBtn->setText(QStringLiteral("退出登录"));
    m_userLabel->setText(QStringLiteral("用户: %1").arg(m_username));
    m_userLabel->setVisible(true);
    m_statusLabel->setText(QStringLiteral("登录成功，正在获取作业列表..."));

    // Save credentials if auto-login was requested via dialog
    if (m_settingsManager && m_pendingAutoLogin && !m_pendingPassword.isEmpty()) {
        m_settingsManager->setOpenJudgeAutoLogin(true);
        m_settingsManager->setOpenJudgeCredentials(m_username, m_pendingPassword);
    }
    m_pendingAutoLogin = false;
    m_pendingPassword.clear();

    emit loginStateChanged(true, m_username);
}

void OpenJudgeWidget::onLoginFailed(const QString &error)
{
    bool wasAutoLogin = m_autoLoginInProgress;
    m_isLoggedIn = false;
    m_autoLoginInProgress = false;
    m_pendingAutoLogin = false;
    m_pendingPassword.clear();
    m_username.clear();

    if (wasAutoLogin) {
        // Auto-login failed, clear saved credentials and fall back to dialog
        if (m_settingsManager) {
            m_settingsManager->setOpenJudgeAutoLogin(false);
            m_settingsManager->clearOpenJudgeCredentials();
        }
        m_statusLabel->setText(QStringLiteral("自动登录失败，请手动登录"));
        onReLogin(); // Will show dialog since tryAutoLogin() will return false
        return;
    }

    m_statusLabel->setText(QStringLiteral("登录失败"));
    QMessageBox::warning(this, QStringLiteral("登录失败"), error);
    m_refreshBtn->setEnabled(true);
}

void OpenJudgeWidget::onLoginLogoutClicked()
{
    if (m_isLoggedIn)
        onLogoutClicked();
    else
        onReLogin();
}

void OpenJudgeWidget::onLogoutClicked()
{
    m_isLoggedIn = false;
    m_username.clear();
    m_autoLoginInProgress = false;
    m_pendingAutoLogin = false;
    m_pendingPassword.clear();
    m_loginBtn->setText(QStringLiteral("登录"));
    m_userLabel->clear();
    m_userLabel->setVisible(false);

    // Clear cookies
    m_crawler->clearCookies();

    // Clear auto-login credentials so it won't auto-login again
    if (m_settingsManager) {
        m_settingsManager->setOpenJudgeAutoLogin(false);
        m_settingsManager->clearOpenJudgeCredentials();
    }

    m_statusLabel->setText(QStringLiteral("已退出登录"));
    m_refreshBtn->setText(QStringLiteral("刷新"));
    m_refreshBtn->setEnabled(false);
    m_backBtn->setVisible(false);
    m_selectBtn->setVisible(false);
    m_viewState = OJ_HOMEWORK_LIST;
    m_listWidget->clear();

    emit loginStateChanged(false, QString());

    // Reload main page anonymously
    m_crawler->fetchMainPage();
}

// ======================================================================
// Main page (two-section: ongoing + past)
// ======================================================================

void OpenJudgeWidget::onMainPageReady(const QList<HomeworkItem> &ongoing,
                                       const QList<HomeworkItem> &past,
                                       const PageInfo &pastPage)
{
    m_ongoingItems = ongoing;
    m_pastPageInfo = pastPage;
    m_viewState = OJ_HOMEWORK_LIST;
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(QStringLiteral("刷新"));
    m_backBtn->setVisible(false);

    if (!pastPage.url.isEmpty()) {
        m_pastItems.clear();
        rebuildListView();
        m_crawler->fetchPastPage(pastPage.url);
    } else {
        m_pastItems = past;
        rebuildListView();
    }
}

void OpenJudgeWidget::onPastPageReady(const QList<HomeworkItem> &past,
                                       const PageInfo &pageInfo)
{
    m_pastItems = past;
    m_pastPageInfo = pageInfo;
    if (m_viewState == OJ_HOMEWORK_LIST)
        rebuildListView();
}

// ======================================================================
// Pagination
// ======================================================================

void OpenJudgeWidget::onPrevPage()
{
    if (!m_pastPageInfo.hasPrev) return;

    QUrl url(m_pastPageInfo.url);
    QUrlQuery query(url);
    int newPage = m_pastPageInfo.currentPage - 1;
    query.removeAllQueryItems(QStringLiteral("page"));
    if (newPage > 1)
        query.addQueryItem(QStringLiteral("page"), QString::number(newPage));
    url.setQuery(query);

    m_statusLabel->setText(QStringLiteral("正在加载第 %1 页...").arg(newPage));
    m_refreshBtn->setEnabled(false);
    m_crawler->fetchPastPage(url.toString());
}

void OpenJudgeWidget::onNextPage()
{
    if (!m_pastPageInfo.hasNext) return;

    QUrl url(m_pastPageInfo.url);
    QUrlQuery query(url);
    int newPage = m_pastPageInfo.currentPage + 1;
    query.removeAllQueryItems(QStringLiteral("page"));
    query.addQueryItem(QStringLiteral("page"), QString::number(newPage));
    url.setQuery(query);

    m_statusLabel->setText(QStringLiteral("正在加载第 %1 页...").arg(newPage));
    m_refreshBtn->setEnabled(false);
    m_crawler->fetchPastPage(url.toString());
}

// ======================================================================
// Rebuild two-section list view
// ======================================================================

void OpenJudgeWidget::rebuildListView()
{
    showListPage();
    m_listWidget->clear();

    // --- 进行中的作业 ---
    if (!m_ongoingItems.isEmpty()) {
        auto *header = new QListWidgetItem(QStringLiteral("=== 进行中的作业 ==="));
        header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
        header->setForeground(ThemeManager::instance().color("syntax.keywords"));
        QFont boldFont = m_listWidget->font();
        boldFont.setBold(true);
        header->setFont(boldFont);
        m_listWidget->addItem(header);

        for (const auto &item : m_ongoingItems) {
            auto *listItem = new QListWidgetItem(item.title);
            listItem->setData(Qt::UserRole, item.url);
            listItem->setData(Qt::UserRole + 1, item.deadline);
            listItem->setSizeHint(QSize(0, 36));
            m_listWidget->addItem(listItem);
        }
    }

    // --- 已结束的作业 ---
    if (!m_pastItems.isEmpty()) {
        // Add a small spacer between sections
        if (!m_ongoingItems.isEmpty()) {
            auto *spacer = new QListWidgetItem(QString());
            spacer->setFlags(Qt::NoItemFlags);
            spacer->setSizeHint(QSize(0, 8));
            m_listWidget->addItem(spacer);
        }

        auto *header = new QListWidgetItem(QStringLiteral("=== 已结束的作业 ==="));
        header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
        header->setForeground(ThemeManager::instance().color("syntax.keywords"));
        QFont boldFont = m_listWidget->font();
        boldFont.setBold(true);
        header->setFont(boldFont);
        m_listWidget->addItem(header);

        for (const auto &item : m_pastItems) {
            auto *listItem = new QListWidgetItem(item.title);
            listItem->setData(Qt::UserRole, item.url);
            listItem->setSizeHint(QSize(0, 36));
            m_listWidget->addItem(listItem);
        }
    }

    // Empty state
    if (m_ongoingItems.isEmpty() && m_pastItems.isEmpty()) {
        auto *emptyItem = new QListWidgetItem(QStringLiteral("(暂无作业条目)"));
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        emptyItem->setForeground(ThemeManager::instance().color("tab.inactiveForeground"));
        m_listWidget->addItem(emptyItem);
    }

    // Pagination bar
    m_pageLabel->setText(QStringLiteral("第 %1 页").arg(m_pastPageInfo.currentPage));
    m_prevPageBtn->setEnabled(m_pastPageInfo.hasPrev);
    m_nextPageBtn->setEnabled(m_pastPageInfo.hasNext);
    m_paginationBar->setVisible(!m_pastItems.isEmpty() || !m_pastPageInfo.url.isEmpty());
}

// ======================================================================
// Homework problems
// ======================================================================

void OpenJudgeWidget::onHomeworkProblemsReady(const QString &homeworkTitle,
                                               const QList<HomeworkItem> &problems)
{
    showListPage();
    m_listWidget->clear();
    m_problemItems = problems;
    m_currentHomeworkTitle = homeworkTitle;
    m_viewState = OJ_PROBLEM_LIST;
    m_backBtn->setVisible(true);
    m_refreshBtn->setText(QStringLiteral("刷新"));
    m_refreshBtn->setEnabled(true);
    m_statusLabel->setText(QString::fromUtf8("题目列表 - %1 (%2 题)")
                           .arg(homeworkTitle).arg(problems.size()));

    if (problems.isEmpty()) {
        auto *emptyItem = new QListWidgetItem(QStringLiteral("(该作业暂无题目)"));
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        emptyItem->setForeground(ThemeManager::instance().color("tab.inactiveForeground"));
        m_listWidget->addItem(emptyItem);
        return;
    }

    for (const auto &problem : problems) {
        auto *listItem = new QListWidgetItem(problem.title);
        listItem->setData(Qt::UserRole, problem.url);
        listItem->setSizeHint(QSize(0, 36));
        m_listWidget->addItem(listItem);
    }
}

// ======================================================================
// Problem detail
// ======================================================================

void OpenJudgeWidget::onProblemDetailReady(const ProblemDetail &detail)
{
    m_currentProblem = detail;
    m_currentProblemSelected = false;
    m_viewState = OJ_PROBLEM_DETAIL;
    m_backBtn->setVisible(true);
    m_refreshBtn->setText(QStringLiteral("刷新"));
    m_refreshBtn->setEnabled(true);
    m_statusLabel->setText(detail.title);

    showDetailPage(detail);
    m_selectBtn->setVisible(true);
    m_selectBtn->setEnabled(true);
    m_selectBtn->setText(QStringLiteral("选择此题目"));
    updateSelectButtonStyle(false);
}

void OpenJudgeWidget::onNetworkError(const QString &error)
{
    m_statusLabel->setText(QStringLiteral("网络错误"));
    QMessageBox::critical(this, QStringLiteral("网络错误"),
                          QStringLiteral("无法连接到服务器:\n") + error);
    m_refreshBtn->setText(QStringLiteral("刷新"));
    m_refreshBtn->setEnabled(true);
}

// ======================================================================
// Item / section clicks
// ======================================================================

bool OpenJudgeWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_listWidget && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        QListWidgetItem *item = m_listWidget->itemAt(mouseEvent->pos());
        if (item && item->data(Qt::UserRole).toString().isEmpty()) {
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void OpenJudgeWidget::onItemClicked(QListWidgetItem *item)
{
    QString url = item->data(Qt::UserRole).toString();
    if (url.isEmpty()) {
        m_listWidget->clearFocus();
        m_listWidget->clearSelection();
        return;
    }

    if (m_viewState == OJ_HOMEWORK_LIST) {
        m_currentHomeworkUrl = url;
        // Check if this homework is ongoing by looking up the URL in m_ongoingItems
        m_currentHomeworkOngoing = false;
        for (const auto &item : m_ongoingItems) {
            if (item.url == url) {
                m_currentHomeworkOngoing = true;
                break;
            }
        }
        m_viewState = OJ_PROBLEM_LIST;
        m_backBtn->setVisible(true);
        m_refreshBtn->setText(QStringLiteral("加载中..."));
        m_refreshBtn->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("正在加载题目..."));
        m_crawler->fetchHomeworkProblems(url);
    } else if (m_viewState == OJ_PROBLEM_LIST) {
        m_currentProblemUrl = url;
        m_refreshBtn->setText(QStringLiteral("加载中..."));
        m_refreshBtn->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("正在加载题目详情..."));
        m_crawler->fetchProblemDetail(url);
    }
}

void OpenJudgeWidget::onSectionClicked(QListWidgetItem *item)
{
    int index = m_sectionList->row(item);
    m_currentSectionIndex = index;
    m_scrollingFromClick = true;
    m_sectionContent->scrollToAnchor(QStringLiteral("section-%1").arg(index));
}

void OpenJudgeWidget::onToggleSidebar()
{
    m_sidebarVisible = !m_sidebarVisible;
    m_sectionList->setVisible(m_sidebarVisible);
    m_toggleSidebarBtn->setText(m_sidebarVisible
        ? QStringLiteral("隐藏栏目") : QStringLiteral("显示栏目"));
    // Re-record positions after layout adjusts to the new width
    if (m_sidebarVisible)
        QTimer::singleShot(0, this, [this]() { recordSectionPositions(); });
}

void OpenJudgeWidget::onToggleProblem()
{
    m_problemVisible = !m_problemVisible;
    if (m_problemVisible) {
        // Restore problem
        m_sectionContent->show();
        if (!m_savedSplitterSizes.isEmpty())
            m_ideSplitter->setSizes(m_savedSplitterSizes);
        m_toggleProblemBtn->setText(QStringLiteral("隐藏题目"));
    } else {
        // Hide problem, editor fills entire area
        m_savedSplitterSizes = m_ideSplitter->sizes();
        m_sectionContent->hide();
        m_toggleProblemBtn->setText(QStringLiteral("显示题目"));
    }
}

// ======================================================================
// Scroll-spy: update left sidebar selection based on scroll position
// ======================================================================

void OpenJudgeWidget::onContentScrolled()
{
    if (m_scrollingFromClick) {
        m_scrollingFromClick = false;
        return;
    }
    if (m_sectionYOffsets.isEmpty()) return;

    int scrollY = m_sectionContent->verticalScrollBar()->value();

    int newIndex = 0;
    for (int i = 0; i < m_sectionYOffsets.size(); ++i) {
        if (m_sectionYOffsets[i] <= scrollY + 10)
            newIndex = i;
        else
            break;
    }

    if (newIndex != m_currentSectionIndex) {
        m_currentSectionIndex = newIndex;
        m_sectionList->blockSignals(true);
        m_sectionList->setCurrentRow(newIndex);
        m_sectionList->blockSignals(false);
    }
}

void OpenJudgeWidget::recordSectionPositions()
{
    m_sectionYOffsets.clear();
    QTextDocument *doc = m_sectionContent->document();
    if (!doc) return;

    QTextCursor searchFrom(doc);
    const auto &sections = m_currentProblem.sections;
    for (int i = 0; i < sections.size(); ++i) {
        QTextCursor found = doc->find(sections[i].heading, searchFrom);
        if (!found.isNull()) {
            QRectF rect = doc->documentLayout()->blockBoundingRect(found.block());
            m_sectionYOffsets.append(static_cast<int>(rect.top()));
            searchFrom = found;
        }
    }
}

// ======================================================================
// Navigation
// ======================================================================

void OpenJudgeWidget::onBack()
{
    if (m_viewState == OJ_PROBLEM_DETAIL) {
        m_viewState = OJ_PROBLEM_LIST;
        m_backBtn->setVisible(true);
        m_refreshBtn->setText(QStringLiteral("刷新"));
        m_refreshBtn->setEnabled(true);
        m_selectBtn->setVisible(false);
        m_statusLabel->setText(QString::fromUtf8("题目列表 - %1").arg(m_currentHomeworkTitle));
        showListPage();
        onHomeworkProblemsReady(m_currentHomeworkTitle, m_problemItems);
    } else if (m_viewState == OJ_PROBLEM_LIST) {
        m_viewState = OJ_HOMEWORK_LIST;
        m_backBtn->setVisible(false);
        m_refreshBtn->setText(QStringLiteral("刷新"));
        m_refreshBtn->setEnabled(true);
        rebuildListView();
    }
}

void OpenJudgeWidget::onRefresh()
{
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(QStringLiteral("加载中..."));

    if (m_viewState == OJ_PROBLEM_LIST && !m_currentHomeworkUrl.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("正在刷新题目列表..."));
        m_crawler->fetchHomeworkProblems(m_currentHomeworkUrl);
    } else if (m_viewState == OJ_PROBLEM_DETAIL && !m_currentProblemUrl.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("正在刷新题目详情..."));
        m_crawler->fetchProblemDetail(m_currentProblemUrl);
    } else {
        m_statusLabel->setText(QStringLiteral("正在刷新..."));
        m_backBtn->setVisible(false);
        m_selectBtn->setVisible(false);
        m_viewState = OJ_HOMEWORK_LIST;
        m_crawler->fetchMainPage();
    }
}

// ======================================================================
// Sample extraction
// ======================================================================

QStringList OpenJudgeWidget::extractPreBlocks(const QString &html)
{
    QStringList result;
    QRegularExpression preRx(
        QStringLiteral("<pre[^>]*>(.*?)</pre>"),
        QRegularExpression::DotMatchesEverythingOption
        | QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = preRx.globalMatch(html);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString content = Crawler::decodeHtmlEntities(match.captured(1).trimmed());
        result.append(content);
    }
    return result;
}

QVector<OpenJudgeWidget::SamplePair>
OpenJudgeWidget::extractSamples(const ProblemDetail &detail)
{
    QStringList inputs, outputs;
    for (const auto &sec : detail.sections) {
        bool isInput = sec.heading.contains(QStringLiteral("样例"))
                       && sec.heading.contains(QStringLiteral("输入"));
        bool isOutput = sec.heading.contains(QStringLiteral("样例"))
                        && sec.heading.contains(QStringLiteral("输出"));
        if (isInput)
            inputs.append(extractPreBlocks(sec.contentHtml));
        else if (isOutput)
            outputs.append(extractPreBlocks(sec.contentHtml));
    }

    int count = qMin(inputs.size(), outputs.size());
    QVector<SamplePair> samples;
    for (int i = 0; i < count; ++i)
        samples.append({inputs[i], outputs[i]});
    return samples;
}

QString OpenJudgeWidget::samplesCacheDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + QStringLiteral("/SM-OJ-Cache/samples/")
           + sanitizeFileName(m_currentProblem.title);
}

bool OpenJudgeWidget::hasCachedSamples() const
{
    QDir dir(samplesCacheDir());
    if (!dir.exists())
        return false;
    QStringList inFiles = dir.entryList({QStringLiteral("*.in")}, QDir::Files);
    return !inFiles.isEmpty();
}

QString OpenJudgeWidget::writeSamplesToCache(const QVector<SamplePair> &samples)
{
    QString cacheDir = samplesCacheDir();

    // Clear only this problem's cache
    QDir dir(cacheDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    QDir().mkpath(cacheDir);

    for (int i = 0; i < samples.size(); ++i) {
        QString baseName = QStringLiteral("test%1").arg(i + 1);
        // Write .in
        QFile inFile(cacheDir + QLatin1Char('/') + baseName + QStringLiteral(".in"));
        if (inFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&inFile);
            ts << samples[i].input;
            ts.flush();
        }
        // Write .out
        QFile outFile(cacheDir + QLatin1Char('/') + baseName + QStringLiteral(".out"));
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&outFile);
            ts << samples[i].output;
            ts.flush();
        }
    }
    return cacheDir;
}

void OpenJudgeWidget::onSelectClicked()
{
    if (m_currentProblemSelected) {
        // Deselect
        m_currentProblemSelected = false;
        m_selectBtn->setText(QStringLiteral("选择此题目"));
        updateSelectButtonStyle(false);
        return;
    }

    QString folderPath;
    if (hasCachedSamples()) {
        folderPath = samplesCacheDir();
    } else {
        QVector<SamplePair> samples = extractSamples(m_currentProblem);
        if (samples.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("该题目没有找到样例输入/输出数据。"));
            return;
        }
        folderPath = writeSamplesToCache(samples);
    }
    emit sampleSelected(folderPath);

    m_currentProblemSelected = true;
    m_selectBtn->setText(QStringLiteral("已选择"));
    updateSelectButtonStyle(true);
}

void OpenJudgeWidget::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    // Toolbar
    if (m_toolbar) {
        m_toolbar->setStyleSheet(QStringLiteral("QWidget { background: %1; }")
                                 .arg(tm.color("activityBar.background").name()));
    }

    // Separator
    if (m_separator) {
        m_separator->setStyleSheet(QStringLiteral("QFrame { color: %1; }")
                                   .arg(tm.color("input.border").name()));
    }

    setStyleSheet(QStringLiteral(
        "OpenJudgeWidget { background: %1; }"
        "QWidget { background: %1; color: %2; }")
        .arg(tm.color("menu.background").name(),
             tm.color("editor.foreground").name()));

    m_userLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; padding: 0 8px;")
                               .arg(tm.color("judge.ac").name()));
    m_pageLabel->setStyleSheet(QStringLiteral("color: %1;").arg(tm.color("editor.foreground").name()));

    auto btnBg = tm.color("input.background");
    auto btnFg = tm.color("input.foreground");
    auto btnBorder = tm.color("input.border");
    auto btnHover = tm.color("aiAssistant.actionButtonHoverBackground");
    auto disabledFg = tm.color("tab.inactiveForeground");
    auto btnQss = buttonStyle(btnBg, btnFg, btnBorder, btnHover, disabledFg);
    m_refreshBtn->setStyleSheet(btnQss);
    m_backBtn->setStyleSheet(btnQss);
    m_loginBtn->setStyleSheet(btnQss);
    m_prevPageBtn->setStyleSheet(btnQss);
    m_nextPageBtn->setStyleSheet(btnQss);
    m_toggleSidebarBtn->setStyleSheet(btnQss);
    m_toggleProblemBtn->setStyleSheet(btnQss);
    m_ideBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 3px; padding: 4px 12px; } "
        "QPushButton:hover { background: %4; } "
        "QPushButton:checked { background: %5; color: white; font-weight: bold; } "
        "QPushButton:disabled { color: %6; }")
        .arg(btnBg.name(), btnFg.name(), btnBorder.name(), btnHover.name(),
             tm.color("editor.selectionBackground").name(), disabledFg.name()));
    updateSelectButtonStyle(m_currentProblemSelected);

    m_listWidget->setStyleSheet(QStringLiteral(
        "QListWidget { color: %1; background: %2; border: none; }"
        "QListWidget::item { padding: 6px 12px; outline: none; }"
        "QListWidget::item:hover { background: %3; }"
        "QListWidget::item:selected { background: %4; color: white; }")
        .arg(tm.color("editor.foreground").name(),
             tm.color("sideBar.background").name(),
             tm.color("list.hoverBackground").name(),
             tm.color("editor.selectionBackground").name()));

    m_sectionList->setStyleSheet(QStringLiteral(
        "QListWidget { color: %1; background: %2; border: none; }"
        "QListWidget::item { padding: 6px 8px; outline: none; }"
        "QListWidget::item:hover { background: %3; }"
        "QListWidget::item:selected { background: %4; color: white; }")
        .arg(tm.color("editor.foreground").name(),
             tm.color("sideBar.background").name(),
             tm.color("list.hoverBackground").name(),
             tm.color("editor.selectionBackground").name()));

    m_sectionContent->setStyleSheet(QStringLiteral(
        "QTextBrowser { padding: 16px; background: %1; border: none; color: %2; }")
        .arg(tm.color("editor.background").name(),
             tm.color("editor.foreground").name()));
    m_sectionContent->document()->setDefaultStyleSheet(QStringLiteral(
        "pre { background-color: %1; padding: 12px; "
        "border: 1px solid %2; font-family: Consolas, monospace; color: %3; }"
        "body { color: %3; }")
        .arg(tm.color("menu.background").name(),
             tm.color("input.border").name(),
             tm.color("editor.foreground").name()));

    // IDE editor container border
    if (m_ideEditorContainer) {
        m_ideEditorContainer->setStyleSheet(QStringLiteral(
            "#ojIdeEditorContainer { background: %1; border-left: 1px solid %2; }")
            .arg(tm.color("editor.background").name(),
                 tm.color("input.border").name()));
    }

    // Language combo
    if (m_langCombo) {
        m_langCombo->setStyleSheet(QStringLiteral(
            "QComboBox { background: %1; color: %2; border: 1px solid %3; "
            "border-radius: 2px; padding: 2px 6px; } "
            "QComboBox::drop-down { border: none; } "
            "QComboBox QAbstractItemView { background: %1; color: %2; "
            "selection-background-color: %4; }")
            .arg(tm.color("input.background").name(),
                 tm.color("input.foreground").name(),
                 tm.color("input.border").name(),
                 tm.color("editor.selectionBackground").name()));
    }

    // Re-render problem detail content with new theme colors
    if (m_viewState == OJ_PROBLEM_DETAIL && !m_currentProblem.sections.isEmpty()) {
        int savedIndex = qMax(0, m_currentSectionIndex);
        m_sectionContent->setHtml(buildCombinedHtml(m_currentProblem));
        m_scrollingFromClick = true;
        m_sectionContent->scrollToAnchor(QStringLiteral("section-%1").arg(savedIndex));
        QTimer::singleShot(0, this, [this]() { recordSectionPositions(); });
    }

}

void OpenJudgeWidget::updateSelectButtonStyle(bool selected)
{
    auto &tm = ThemeManager::instance();
    if (selected) {
        m_selectBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; font-weight: bold; "
            "border: none; border-radius: 4px; padding: 6px 20px; } "
            "QPushButton:hover { background: %2; } "
            "QPushButton:disabled { background: %3; color: %4; }")
            .arg(tm.color("badge.background").lighter(130).name(),
                 tm.color("badge.background").lighter(145).name(),
                 tm.color("input.background").name(),
                 tm.color("tab.inactiveForeground").name()));
    } else {
        m_selectBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; font-weight: bold; "
            "border: none; border-radius: 4px; padding: 6px 20px; } "
            "QPushButton:hover { background: %2; } "
            "QPushButton:disabled { background: %3; color: %4; }")
            .arg(tm.color("badge.background").name(),
                 tm.color("badge.background").lighter(115).name(),
                 tm.color("input.background").name(),
                 tm.color("tab.inactiveForeground").name()));
    }
}

void OpenJudgeWidget::submitCurrentProblem(const QString &sourceCode, int languageId)
{
    if (m_currentProblemUrl.isEmpty()) {
        emit submissionFailed(QStringLiteral("请先在 OpenJudge 中选择一道题目"));
        return;
    }
    if (!m_currentProblemSelected) {
        emit submissionFailed(QStringLiteral("请先在 OpenJudge 中选择一道题目"));
        return;
    }
    if (!m_isLoggedIn) {
        emit submissionFailed(QStringLiteral("请先登录 OpenJudge"));
        return;
    }
    if (!m_currentHomeworkOngoing) {
        emit submissionFailed(QStringLiteral("该作业已结束，无法提交"));
        return;
    }
    m_statusLabel->setText(QStringLiteral("正在提交..."));
    saveLastIdeLanguage(languageId);
    m_crawler->submitCode(m_currentProblemUrl, sourceCode, languageId);
}

// ======================================================================
// IDE Mode
// ======================================================================

QVector<OpenJudgeWidget::OjLangOption> OpenJudgeWidget::ideLanguageOptions() const
{
    return {
        {QStringLiteral("C++ (G++)"),  1, QStringLiteral("cpp"),    QStringLiteral(".cpp")},
        {QStringLiteral("Python 3"),   6, QStringLiteral("python"), QStringLiteral(".py")},
    };
}

QString OpenJudgeWidget::sanitizeFileName(const QString &name) const
{
    QString result = name;
    static const QRegularExpression invalid(QStringLiteral("[\\\\/:*?\"<>|]"));
    result.replace(invalid, QStringLiteral("_"));
    if (result.length() > 64)
        result = result.left(64);
    return result;
}

QString OpenJudgeWidget::ideCacheDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + QStringLiteral("/SM-OJ-Cache/oj_ide");
}

QString OpenJudgeWidget::ideCacheFilePath() const
{
    auto opts = ideLanguageOptions();
    QString ext = QStringLiteral(".cpp");
    for (const auto &opt : opts) {
        if (opt.ojId == m_currentLangId) {
            ext = opt.ext;
            break;
        }
    }
    QString baseName = m_currentProblem.title.isEmpty()
        ? QStringLiteral("untitled")
        : sanitizeFileName(m_currentProblem.title);
    return ideCacheDir() + QStringLiteral("/") + baseName + ext;
}

QString OpenJudgeWidget::ideLangCacheFilePath() const
{
    return ideCacheDir() + QStringLiteral("/lang_prefs.json");
}

void OpenJudgeWidget::saveLastIdeLanguage(int langId)
{
    if (m_currentProblem.title.isEmpty())
        return;

    int id = (langId >= 0) ? langId : m_currentLangId;

    QDir().mkpath(ideCacheDir());
    QString path = ideLangCacheFilePath();

    // Read existing prefs
    QJsonObject root;
    QFile readFile(path);
    if (readFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(readFile.readAll());
        if (doc.isObject())
            root = doc.object();
        readFile.close();
    }

    // Save both per-problem and global keys
    root[sanitizeFileName(m_currentProblem.title)] = id;
    root[QStringLiteral("__global__")] = id;

    QFile writeFile(path);
    if (writeFile.open(QIODevice::WriteOnly)) {
        writeFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}

int OpenJudgeWidget::loadLastIdeLanguage() const
{
    if (m_currentProblem.title.isEmpty())
        return -1;

    QString path = ideLangCacheFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return -1;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject())
        return -1;

    QJsonObject root = doc.object();

    // Per-problem preference takes priority
    QString key = sanitizeFileName(m_currentProblem.title);
    if (root.contains(key))
        return root[key].toInt(-1);

    // Fall back to global last-submitted language
    if (root.contains(QStringLiteral("__global__")))
        return root[QStringLiteral("__global__")].toInt(-1);

    return -1;
}

QString OpenJudgeWidget::ideCode() const
{
    if (m_ideCodeEditor)
        return m_ideCodeEditor->toPlainText();
    return QString();
}

void OpenJudgeWidget::saveIdeCodeToCache()
{
    if (!m_ideCodeEditor)
        return;
    QDir().mkpath(ideCacheDir());
    QString path = ideCacheFilePath();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_ideCodeEditor->toPlainText();
    }
}

void OpenJudgeWidget::loadIdeCodeFromCache()
{
    if (!m_ideCodeEditor)
        return;
    QString path = ideCacheFilePath();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        m_ideCodeEditor->setPlainText(in.readAll());
        m_ideCodeEditor->document()->setModified(false);
    } else {
        m_ideCodeEditor->clear();
        m_ideCodeEditor->document()->setModified(false);
    }
}

void OpenJudgeWidget::selectIdeLanguage()
{
    if (!m_langCombo || !m_ideCodeEditor)
        return;

    const auto opts = ideLanguageOptions();

    // Determine best language:
    // 1. If problem has exactly one available (supported) language → use it
    // 2. Else if there's a last-used language (per-problem or global) → use it
    // 3. Otherwise → first option (C++)
    int defaultOjId = opts.first().ojId;
    const auto &avail = m_currentProblem.availableLanguages;
    QList<int> supportedAvail;
    for (int langId : avail) {
        if (m_langCombo->findData(langId) >= 0)
            supportedAvail.append(langId);
    }
    if (supportedAvail.size() == 1) {
        defaultOjId = supportedAvail.first();
    } else {
        int lastLang = loadLastIdeLanguage();
        if (lastLang > 0 && m_langCombo->findData(lastLang) >= 0) {
            if (supportedAvail.isEmpty() || supportedAvail.contains(lastLang))
                defaultOjId = lastLang;
        }
    }

    // Update combo (block signals to avoid triggering onIdeLanguageChanged)
    int defaultIdx = m_langCombo->findData(defaultOjId);
    if (defaultIdx >= 0) {
        m_langCombo->blockSignals(true);
        m_langCombo->setCurrentIndex(defaultIdx);
        m_langCombo->blockSignals(false);
    }

    // Apply language to editor
    m_currentLangId = defaultOjId;
    QString codeLang = QStringLiteral("cpp");
    for (const auto &opt : opts) {
        if (opt.ojId == m_currentLangId) {
            codeLang = opt.codeLang;
            break;
        }
    }
    m_currentCodeLangId = codeLang;
    m_ideCodeEditor->setLanguage(codeLang);
}

void OpenJudgeWidget::setupIdeMode()
{
    // Create editor container with code editor
    m_ideEditorContainer = new QWidget;
    m_ideEditorContainer->setObjectName(QStringLiteral("ojIdeEditorContainer"));
    auto &tm = ThemeManager::instance();
    m_ideEditorContainer->setStyleSheet(QStringLiteral(
        "#ojIdeEditorContainer { background: %1; border-left: 1px solid %2; }")
        .arg(tm.color("editor.background").name(),
             tm.color("input.border").name()));

    auto *editorLayout = new QVBoxLayout(m_ideEditorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    m_ideCodeEditor = new CodeEditor;
    m_ideCodeEditor->setIndentWidth(4);
    connect(&tm, &ThemeManager::themeChanged, m_ideCodeEditor, &CodeEditor::reloadColors);
    connect(m_ideCodeEditor, &CodeEditor::diagnosticsToggleRequested,
            this, &OpenJudgeWidget::ideDiagnosticsToggleRequested);
    editorLayout->addWidget(m_ideCodeEditor, 1);

    // Populate language combo (once)
    const auto opts = ideLanguageOptions();
    m_langCombo->blockSignals(true);
    for (const auto &opt : opts)
        m_langCombo->addItem(opt.display, opt.ojId);
    m_langCombo->blockSignals(false);

    // Select language for current problem
    selectIdeLanguage();

    // Splitter ratio clamp (3:7 ~ 7:3)
    connect(m_ideSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        QList<int> sizes = m_ideSplitter->sizes();
        if (sizes.size() < 2)
            return;
        int total = sizes.at(0) + sizes.at(1);
        if (total <= 0)
            return;
        double ratio = (double)sizes.at(0) / total;
        if (ratio < 0.3) {
            sizes[0] = (int)(total * 0.3);
            sizes[1] = total - sizes[0];
            m_ideSplitter->setSizes(sizes);
        } else if (ratio > 0.7) {
            sizes[0] = (int)(total * 0.7);
            sizes[1] = total - sizes[0];
            m_ideSplitter->setSizes(sizes);
        }
    });
}

void OpenJudgeWidget::onToggleIdeMode()
{
    if (m_currentProblemUrl.isEmpty() || m_currentProblem.sections.isEmpty()) {
        m_ideBtn->setChecked(false);
        return;
    }

    m_ideMode = !m_ideMode;

    if (m_ideMode) {
        // ENTER IDE mode

        // Auto-select problem if not already selected
        if (!m_currentProblemSelected) {
            QString folderPath;
            if (hasCachedSamples()) {
                folderPath = samplesCacheDir();
            } else {
                QVector<SamplePair> samples = extractSamples(m_currentProblem);
                if (!samples.isEmpty()) {
                    folderPath = writeSamplesToCache(samples);
                }
            }
            if (!folderPath.isEmpty()) {
                emit sampleSelected(folderPath);
                m_currentProblemSelected = true;
                m_selectBtn->setText(QStringLiteral("已选择"));
                updateSelectButtonStyle(true);
            }
        }

        // Lazy-create editor on first use
        if (!m_ideEditorContainer) {
            setupIdeMode();
        } else {
            // Re-evaluate language for current problem (may differ from previous)
            selectIdeLanguage();
        }

        // Add editor container to splitter (right side of problem)
        m_ideSplitter->addWidget(m_ideEditorContainer);
        m_ideEditorContainer->show();

        // Set initial split (50:50)
        int total = m_ideSplitter->width();
        m_ideSplitter->setSizes({total / 2, total / 2});

        // Show language combo
        m_langCombo->setVisible(true);
        m_toggleProblemBtn->setVisible(true);
        m_ideBtn->setChecked(true);
        emit ideModeChanged(true);

        // Load cached code for this problem (after language selection)
        loadIdeCodeFromCache();
        if (m_ideCodeEditor) {
            m_ideCodeEditor->setFocus();
        }
    } else {
        // EXIT IDE mode
        // Save code to cache
        saveIdeCodeToCache();

        // Hide editor from splitter
        if (m_ideEditorContainer) {
            m_ideEditorContainer->hide();
        }

        // Hide language combo
        m_langCombo->setVisible(false);
        m_toggleProblemBtn->setVisible(false);
        // Restore problem visibility if hidden
        if (!m_problemVisible) {
            m_sectionContent->show();
            m_problemVisible = true;
            m_toggleProblemBtn->setText(QStringLiteral("隐藏题目"));
        }
        m_ideBtn->setChecked(false);
        emit ideModeChanged(false);
    }
}

void OpenJudgeWidget::onIdeLanguageChanged(int index)
{
    if (index < 0 || !m_ideCodeEditor || m_ideLangChanging)
        return;

    int newOjId = m_langCombo->itemData(index).toInt();
    if (newOjId == m_currentLangId)
        return;

    m_ideLangChanging = true;

    // Save current code under old extension
    saveIdeCodeToCache();

    m_currentLangId = newOjId;

    // Update editor language
    const auto opts = ideLanguageOptions();
    for (const auto &opt : opts) {
        if (opt.ojId == m_currentLangId) {
            m_currentCodeLangId = opt.codeLang;
            m_ideCodeEditor->setLanguage(opt.codeLang);
            break;
        }
    }

    // Delete old cache file if different extension
    loadIdeCodeFromCache();

    saveLastIdeLanguage();

    m_ideLangChanging = false;
}
