#include "openjudgewindow.h"
#include "logindialog.h"
#include "settingsmanager.h"
#include "configmanager.h"
#include "thememanager.h"

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
#include <QTextStream>
#include <QStandardPaths>

// ===== HomeworkDelegate: draws deadline right-aligned =====
class HomeworkDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);

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

// ===== HTML wrapping helper =====
static QString wrapHtml(const QString &body, const QColor &bg, const QColor &fg,
                        const QColor &preBg, const QColor &border, const QColor &link)
{
    QString html = QStringLiteral(
        "<html><head><meta charset=\"utf-8\"><style>"
        "body{background:%1;color:%2;font-family:'Microsoft YaHei',sans-serif;font-size:10pt;}"
        "pre{background:%3;padding:12px;border:1px solid %4;font-family:Consolas,monospace;color:%2;}"
        "a{color:%5;}"
        "</style></head><body>").arg(bg.name(), fg.name(), preBg.name(), border.name(), link.name());
    html += body;
    html += QStringLiteral("</body></html>");
    return html;
}

// ======================================================================
// Constructor
// ======================================================================

OpenJudgeWindow::OpenJudgeWindow(SettingsManager *settings, QWidget *parent)
    : QMainWindow(parent)
    , m_crawler(new Crawler(this))
    , m_settingsManager(settings)
    , m_listWidget(new QListWidget)
    , m_statusLabel(new QLabel)
    , m_refreshBtn(new QPushButton(QStringLiteral("刷新")))
    , m_backBtn(new QPushButton(QStringLiteral("← 返回")))
    , m_selectBtn(new QPushButton(QStringLiteral("选择此题目")))
    , m_stackedWidget(new QStackedWidget)
    , m_detailPage(new QWidget)
    , m_sectionList(new QListWidget)
    , m_sectionContent(new QTextBrowser)
{
    setWindowTitle(QStringLiteral("OpenJudge 题目浏览"));
    resize(ConfigManager::instance().openJudgeWindowWidth(),
           ConfigManager::instance().openJudgeWindowHeight());

    setupUi();

    // --- Crawler signals ---
    connect(m_crawler, &Crawler::loginSuccess, this, &OpenJudgeWindow::onLoginSuccess);
    connect(m_crawler, &Crawler::loginFailed, this, &OpenJudgeWindow::onLoginFailed);
    connect(m_crawler, &Crawler::mainPageReady, this, &OpenJudgeWindow::onMainPageReady);
    connect(m_crawler, &Crawler::pastPageReady, this, &OpenJudgeWindow::onPastPageReady);
    connect(m_crawler, &Crawler::homeworkProblemsReady, this, &OpenJudgeWindow::onHomeworkProblemsReady);
    connect(m_crawler, &Crawler::problemDetailReady, this, &OpenJudgeWindow::onProblemDetailReady);
    connect(m_crawler, &Crawler::networkError, this, &OpenJudgeWindow::onNetworkError);

    // --- UI signals ---
    connect(m_listWidget, &QListWidget::itemClicked, this, &OpenJudgeWindow::onItemClicked);
    connect(m_sectionList, &QListWidget::itemClicked, this, &OpenJudgeWindow::onSectionClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &OpenJudgeWindow::onRefresh);
    connect(m_backBtn, &QPushButton::clicked, this, &OpenJudgeWindow::onBack);
    connect(m_prevPageBtn, &QPushButton::clicked, this, &OpenJudgeWindow::onPrevPage);
    connect(m_nextPageBtn, &QPushButton::clicked, this, &OpenJudgeWindow::onNextPage);
    connect(m_selectBtn, &QPushButton::clicked, this, &OpenJudgeWindow::onSelectClicked);

    // --- Crawler submission signals (forwarded) ---
    connect(m_crawler, &Crawler::submissionResultReady,
            this, &OpenJudgeWindow::submissionResultReady);
    connect(m_crawler, &Crawler::submissionFailed, this, [this](const QString &error) {
        m_refreshBtn->setEnabled(true);
        m_statusLabel->setText(QStringLiteral("提交失败"));
        emit submissionFailed(error);
    });
    connect(m_crawler, &Crawler::submitPollTimeout, this, [this]() {
        m_statusLabel->setText(QStringLiteral("提交结果获取超时"));
    });

    // Login dialog is triggered by MainWindow::onOpenJudgeRequested after window is shown

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &OpenJudgeWindow::refreshStyle);
}

// ======================================================================
// UI Construction — dark theme
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

void OpenJudgeWindow::setupUi()
{
    auto &tm = ThemeManager::instance();
    auto btnBg = tm.color("input.background");
    auto btnFg = tm.color("input.foreground");
    auto btnBorder = tm.color("input.border");
    auto btnHover = tm.color("aiAssistant.actionButtonHoverBackground");
    auto disabledFg = tm.color("tab.inactiveForeground");

    setStyleSheet(QStringLiteral(
        "QMainWindow { background: %1; }"
        "QWidget { background: %1; color: %2; }")
        .arg(tm.color("menu.background").name(),
             tm.color("editor.foreground").name()));

    // --- Top toolbar ---
    auto *toolbar = new QWidget;
    toolbar->setStyleSheet(QStringLiteral("QWidget { background: %1; }")
                           .arg(tm.color("activityBar.background").name()));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
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
             tm.color("button.hoverBackground").name(),
             btnBg.name(), disabledFg.name()));
    m_selectBtn->setVisible(false);

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
    connect(m_loginBtn, &QPushButton::clicked, this, &OpenJudgeWindow::onLoginLogoutClicked);

    toolbarLayout->addWidget(m_selectBtn);
    toolbarLayout->addWidget(m_backBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_refreshBtn);
    toolbarLayout->addWidget(m_userLabel);
    toolbarLayout->addWidget(m_loginBtn);

    // --- Separator ---
    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet(QStringLiteral("QFrame { color: %1; }").arg(tm.color("input.border").name()));

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

    // --- Central widget ---
    auto *centralWidget = new QWidget;
    auto *centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(toolbar);
    centralLayout->addWidget(separator);
    centralLayout->addWidget(m_stackedWidget, 1);
    setCentralWidget(centralWidget);

    // --- Style the list ---
    m_listWidget->setFont(QFont("Microsoft YaHei", 10));
    m_listWidget->setSpacing(4);
    m_listWidget->setCursor(Qt::PointingHandCursor);
    m_listWidget->setAlternatingRowColors(false);
    m_listWidget->setItemDelegate(new HomeworkDelegate(m_listWidget));
    m_listWidget->setStyleSheet(QStringLiteral(
        "QListWidget { color: %1; background: %2; border: none; }"
        "QListWidget::item { padding: 6px 12px; }"
        "QListWidget::item:hover { background: %3; }"
        "QListWidget::item:selected { background: %4; color: white; }")
        .arg(tm.color("editor.foreground").name(),
             tm.color("sideBar.background").name(),
             tm.color("list.hoverBackground").name(),
             tm.color("editor.selectionBackground").name()));
}

void OpenJudgeWindow::setupDetailPage()
{
    auto &tm = ThemeManager::instance();
    auto *layout = new QHBoxLayout(m_detailPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Left: section list — tight fit to content
    m_sectionList->setFont(QFont("Microsoft YaHei", 10));
    m_sectionList->setFixedWidth(ConfigManager::instance().openJudgeSectionListWidth());
    m_sectionList->setSpacing(2);
    m_sectionList->setCursor(Qt::PointingHandCursor);
    m_sectionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sectionList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_sectionList->setStyleSheet(QStringLiteral(
        "QListWidget { color: %1; background: %2; border: none; }"
        "QListWidget::item { padding: 6px 8px; }"
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

    layout->addWidget(m_sectionList);
    layout->addWidget(m_sectionContent, 1);
}

// ======================================================================
// View switching
// ======================================================================

void OpenJudgeWindow::showListPage()
{
    m_stackedWidget->setCurrentIndex(0);
    m_selectBtn->setVisible(false);
}

void OpenJudgeWindow::showDetailPage(const ProblemDetail &detail)
{
    m_sectionList->clear();
    m_sectionContent->clear();

    for (const auto &sec : detail.sections) {
        auto *item = new QListWidgetItem(sec.heading);
        item->setData(Qt::UserRole, sec.contentHtml);
        item->setSizeHint(QSize(0, 32));
        m_sectionList->addItem(item);
    }

    if (!detail.sections.isEmpty()) {
        m_sectionList->setCurrentRow(0);
    {
        auto &tm = ThemeManager::instance();
        m_sectionContent->setHtml(wrapHtml(detail.sections.first().contentHtml,
            tm.color("editor.background"), tm.color("editor.foreground"),
            tm.color("menu.background"), tm.color("input.border"),
            tm.color("syntax.keywords")));
    }
    }

    m_stackedWidget->setCurrentIndex(1);
}

// ======================================================================
// Login / auth
// ======================================================================

bool OpenJudgeWindow::tryAutoLogin()
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

void OpenJudgeWindow::onReLogin()
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

void OpenJudgeWindow::onLoginSuccess()
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

void OpenJudgeWindow::onLoginFailed(const QString &error)
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

void OpenJudgeWindow::onLoginLogoutClicked()
{
    if (m_isLoggedIn)
        onLogoutClicked();
    else
        onReLogin();
}

void OpenJudgeWindow::onLogoutClicked()
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

void OpenJudgeWindow::onMainPageReady(const QList<HomeworkItem> &ongoing,
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

void OpenJudgeWindow::onPastPageReady(const QList<HomeworkItem> &past,
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

void OpenJudgeWindow::onPrevPage()
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

void OpenJudgeWindow::onNextPage()
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

void OpenJudgeWindow::rebuildListView()
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

void OpenJudgeWindow::onHomeworkProblemsReady(const QString &homeworkTitle,
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

void OpenJudgeWindow::onProblemDetailReady(const ProblemDetail &detail)
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

void OpenJudgeWindow::onNetworkError(const QString &error)
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

void OpenJudgeWindow::onItemClicked(QListWidgetItem *item)
{
    QString url = item->data(Qt::UserRole).toString();
    if (url.isEmpty()) return;

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

void OpenJudgeWindow::onSectionClicked(QListWidgetItem *item)
{
    QString content = item->data(Qt::UserRole).toString();
    {
        auto &tm = ThemeManager::instance();
        m_sectionContent->setHtml(wrapHtml(content,
            tm.color("editor.background"), tm.color("editor.foreground"),
            tm.color("menu.background"), tm.color("input.border"),
            tm.color("syntax.keywords")));
    }
}

// ======================================================================
// Navigation
// ======================================================================

void OpenJudgeWindow::onBack()
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

void OpenJudgeWindow::onRefresh()
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

QStringList OpenJudgeWindow::extractPreBlocks(const QString &html)
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

QVector<OpenJudgeWindow::SamplePair>
OpenJudgeWindow::extractSamples(const ProblemDetail &detail)
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

QString OpenJudgeWindow::writeSamplesToCache(const QVector<SamplePair> &samples)
{
    QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString cacheDir = tempRoot + QStringLiteral("/SM-OJ-Cache");

    // Clear existing contents and recreate
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

void OpenJudgeWindow::onSelectClicked()
{
    if (m_currentProblemSelected) {
        // Deselect
        m_currentProblemSelected = false;
        m_selectBtn->setText(QStringLiteral("选择此题目"));
        updateSelectButtonStyle(false);
        return;
    }

    QVector<SamplePair> samples = extractSamples(m_currentProblem);
    if (samples.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("该题目没有找到样例输入/输出数据。"));
        return;
    }
    QString folderPath = writeSamplesToCache(samples);
    emit sampleSelected(folderPath);

    m_currentProblemSelected = true;
    m_selectBtn->setText(QStringLiteral("已选择"));
    updateSelectButtonStyle(true);
}

void OpenJudgeWindow::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QStringLiteral(
        "QMainWindow { background: %1; }"
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
    updateSelectButtonStyle(m_currentProblemSelected);

    m_listWidget->setStyleSheet(QStringLiteral(
        "QListWidget { color: %1; background: %2; border: none; }"
        "QListWidget::item { padding: 6px 12px; }"
        "QListWidget::item:hover { background: %3; }"
        "QListWidget::item:selected { background: %4; color: white; }")
        .arg(tm.color("editor.foreground").name(),
             tm.color("sideBar.background").name(),
             tm.color("list.hoverBackground").name(),
             tm.color("editor.selectionBackground").name()));

    m_sectionList->setStyleSheet(QStringLiteral(
        "QListWidget { color: %1; background: %2; border: none; }"
        "QListWidget::item { padding: 6px 8px; }"
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

}

void OpenJudgeWindow::updateSelectButtonStyle(bool selected)
{
    auto &tm = ThemeManager::instance();
    if (selected) {
        m_selectBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; font-weight: bold; "
            "border: none; border-radius: 4px; padding: 6px 20px; } "
            "QPushButton:hover { background: %2; } "
            "QPushButton:disabled { background: %3; color: %4; }")
            .arg(tm.color("button.background").lighter(130).name(),
                 tm.color("button.hoverBackground").lighter(130).name(),
                 tm.color("input.background").name(),
                 tm.color("tab.inactiveForeground").name()));
    } else {
        m_selectBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white; font-weight: bold; "
            "border: none; border-radius: 4px; padding: 6px 20px; } "
            "QPushButton:hover { background: %2; } "
            "QPushButton:disabled { background: %3; color: %4; }")
            .arg(tm.color("badge.background").name(),
                 tm.color("button.hoverBackground").name(),
                 tm.color("input.background").name(),
                 tm.color("tab.inactiveForeground").name()));
    }
}

void OpenJudgeWindow::submitCurrentProblem(const QString &sourceCode, int languageId)
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
    m_crawler->submitCode(m_currentProblemUrl, sourceCode, languageId);
}
