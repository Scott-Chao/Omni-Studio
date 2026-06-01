#include "aipanel.h"
#include "chatarea.h"
#include "actionbar.h"
#include "errorlistpanel.h"
#include "errorjournal.h"
#include "aihistorylistwidget.h"
#include "core/thememanager.h"
#include "widgets/tabbuttongroup.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>

// ═══════════════════════════════════════════════════════════════════════

AiPanel::AiPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(DefaultWidth);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Title bar with tab buttons ──
    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(36);

    auto *titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(4, 0, 6, 0);
    titleLayout->setSpacing(2);

    m_aiTabBtn = new QPushButton(tr("AI 助手"));
    m_aiTabBtn->setFixedHeight(32);
    m_aiTabBtn->setCursor(Qt::PointingHandCursor);

    m_errorTabBtn = new QPushButton(tr("错题本"));
    m_errorTabBtn->setFixedHeight(32);
    m_errorTabBtn->setCursor(Qt::PointingHandCursor);

    m_historyTabBtn = new QPushButton(tr("历史对话"));
    m_historyTabBtn->setFixedHeight(32);
    m_historyTabBtn->setCursor(Qt::PointingHandCursor);

    m_newConvBtn = new QPushButton(tr("新对话"));
    m_newConvBtn->setFixedHeight(24);
    m_newConvBtn->setCursor(Qt::PointingHandCursor);

    m_clearBtn = new QPushButton(tr("清空对话"));
    m_clearBtn->setFixedHeight(24);
    m_clearBtn->setCursor(Qt::PointingHandCursor);

    titleLayout->addWidget(m_aiTabBtn);
    titleLayout->addWidget(m_errorTabBtn);
    titleLayout->addWidget(m_historyTabBtn);
    titleLayout->addStretch();
    titleLayout->addWidget(m_newConvBtn);
    titleLayout->addWidget(m_clearBtn);

    // ── Action bar (only shown on chat tab) ──
    m_actionBar = new ActionBar(this);

    // ── Stacked widget: chat area (0) | error list (1) | history list (2) ──
    m_stackedWidget = new QStackedWidget(this);

    m_chatArea = new ChatArea(this);
    m_errorListPanel = new ErrorListPanel(this);
    m_historyListWidget = new AiHistoryListWidget(this);

    m_stackedWidget->addWidget(m_chatArea);          // index 0 = ChatTab
    m_stackedWidget->addWidget(m_errorListPanel);     // index 1 = ErrorTab
    m_stackedWidget->addWidget(m_historyListWidget);  // index 2 = HistoryTab

    // ── Input bar (only shown on chat tab) ──
    m_inputBar = new QWidget(this);
    auto *inputLayout = new QHBoxLayout(m_inputBar);
    inputLayout->setContentsMargins(6, 6, 6, 6);
    inputLayout->setSpacing(6);

    m_inputEdit = new QLineEdit;
    m_inputEdit->setPlaceholderText(tr("输入消息..."));

    m_sendBtn = new QPushButton(tr("发送"));
    m_sendBtn->setFixedSize(50, 28);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setEnabled(false);

    inputLayout->addWidget(m_inputEdit, 1);
    inputLayout->addWidget(m_sendBtn);

    // ── Assemble main layout ──
    mainLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_actionBar);
    mainLayout->addWidget(m_stackedWidget, 1);
    mainLayout->addWidget(m_inputBar);

    // ── Tab button group (starts on chat tab) ──
    m_tabGroup = new TabButtonGroup(m_stackedWidget, this);
    m_tabGroup->addTab(m_aiTabBtn, ChatTab);
    m_tabGroup->addTab(m_errorTabBtn, ErrorTab);
    m_tabGroup->addTab(m_historyTabBtn, HistoryTab);
    m_tabGroup->setStyleProvider(&AiPanel::tabButtonStyle);
    connect(m_tabGroup, &TabButtonGroup::currentChanged, this, &AiPanel::onTabSwitch);
    m_tabGroup->setCurrentIndex(ChatTab);

    connect(m_actionBar, &ActionBar::actionTriggered, this, [this](AiAction action) {
        emit actionTriggered(static_cast<int>(action));
    });

    auto doSend = [this]() {
        QString text = m_inputEdit->text().trimmed();
        if (!text.isEmpty()) {
            emit sendMessage(text);
            m_inputEdit->clear();
        }
    };

    connect(m_inputEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_sendBtn->setEnabled(!text.trimmed().isEmpty());
    });
    connect(m_inputEdit, &QLineEdit::returnPressed, this, [doSend]() {
        doSend();
    });
    connect(m_sendBtn, &QPushButton::clicked, this, [doSend]() {
        doSend();
    });
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        clearChat();
        emit clearRequested();
    });
    connect(m_newConvBtn, &QPushButton::clicked, this, [this]() {
        clearChat();
        emit newConversationRequested();
    });

    // Forward error list item click
    connect(m_errorListPanel, &ErrorListPanel::errorClicked,
            this, &AiPanel::errorSelected);

    // Connect ErrorJournal analysis ready to error list panel update
    connect(&ErrorJournal::instance(), &ErrorJournal::analysisReady,
            m_errorListPanel, &ErrorListPanel::updateAnalysis);

    // Delete actions
    connect(m_errorListPanel, &ErrorListPanel::deleteAllRequested, this, [this]() {
        ErrorJournal::instance().clearAll();
        if (m_tabGroup->currentIndex() == ErrorTab)
            m_errorListPanel->loadRecords();
    });
    connect(m_errorListPanel, &ErrorListPanel::deleteRecordRequested, this, [this](const QString &recordId) {
        ErrorJournal::instance().deleteRecord(recordId);
        if (m_tabGroup->currentIndex() == ErrorTab)
            m_errorListPanel->loadRecords();
    });

    // ── Error journal badge tracking ──
    connect(&ErrorJournal::instance(), &ErrorJournal::recordsChanged,
            this, [this]() {
        updateErrorBadge();
        if (m_tabGroup->currentIndex() == ErrorTab)
            m_errorListPanel->loadRecords();
    });

    // Show initial badge count
    updateErrorBadge();

    // ── Theme support ──
    ThemeManager::watchTheme(this, &AiPanel::refreshStyle);
    refreshStyle();
}

void AiPanel::onTabSwitch(int index)
{
    // Show/hide chat-specific UI
    bool isChat = (index == ChatTab);
    m_actionBar->setVisible(isChat);
    m_inputBar->setVisible(isChat);
    m_newConvBtn->setVisible(isChat);
    m_clearBtn->setVisible(isChat);

    if (index == ErrorTab) {
        m_errorListPanel->loadRecords();
    } else if (index == HistoryTab) {
        emit historyListVisibilityChanged(true);
    }
}

// ── Static tab button style provider ──

QString AiPanel::tabButtonStyle(int /*index*/, bool active)
{
    auto &tm = ThemeManager::instance();
    if (active) {
        return QStringLiteral(
            "QPushButton {"
            "  background: transparent;"
            "  color: %1;"
            "  border: none;"
            "  border-bottom: 2px solid %2;"
            "  font-size: 12px;"
            "  font-weight: bold;"
            "  padding: 0 10px;"
            "}"
        ).arg(tm.color("workbench.foreground").name(),
              tm.color("activityBar.activeBorder").name());
    }
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  font-size: 12px;"
        "  font-weight: normal;"
        "  padding: 0 10px;"
        "}"
        "QPushButton:hover {"
        "  color: %2;"
        "}"
    ).arg(tm.color("tab.inactiveForeground").name(),
          tm.color("editorLineNumber.foreground").name());
}

void AiPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    m_titleBar->setStyleSheet(QStringLiteral(
        "background-color: %1;"
    ).arg(tm.color("editorLineNumber.background").name()));

    m_clearBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  border: 1px solid %1;"
        "  border-radius: 4px;"
        "  color: %2;"
        "  font-size: 11px;"
        "  padding: 0 8px;"
        "}"
        "QPushButton:hover {"
        "  color: %3;"
        "  border-color: %4;"
        "}"
    ).arg(tm.color("input.border").name(),
          tm.color("editorLineNumber.foreground").name(),
          tm.color("workbench.foreground").name(),
          tm.color("input.foreground").name()));

    m_newConvBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  border: 1px solid %1;"
        "  border-radius: 4px;"
        "  color: %2;"
        "  font-size: 11px;"
        "  padding: 0 8px;"
        "}"
        "QPushButton:hover {"
        "  color: %3;"
        "  border-color: %4;"
        "}"
    ).arg(tm.color("input.border").name(),
          tm.color("editorLineNumber.foreground").name(),
          tm.color("workbench.foreground").name(),
          tm.color("input.foreground").name()));

    m_inputBar->setStyleSheet(QStringLiteral(
        "background-color: %1;"
    ).arg(tm.color("editorLineNumber.background").name()));

    m_inputEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  padding: 6px 8px;"
        "  font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: %4;"
        "}"
    ).arg(tm.color("input.background").name(),
          tm.color("input.foreground").name(),
          tm.color("input.border").name(),
          tm.color("activityBar.activeBorder").name()));

    m_sendBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3;"
        "}"
        "QPushButton:disabled {"
        "  background-color: %4;"
        "  color: %5;"
        "}"
    ).arg(tm.color("badge.background").name(),
          tm.color("badge.foreground").name(),
          tm.color("badge.background").lighter(115).name(),
          tm.color("input.background").name(),
          tm.color("editorLineNumber.foreground").name()));

    m_tabGroup->refreshStyles();
}

void AiPanel::updateErrorBadge()
{
    int count = ErrorJournal::instance().recordCount();
    if (count > 0)
        m_errorTabBtn->setText(tr("错题本 (%1)").arg(count));
    else
        m_errorTabBtn->setText(tr("错题本"));
}

void AiPanel::setCurrentTab(int index)
{
    m_tabGroup->setCurrentIndex(index);
}

// ── Chat operations (delegated to ChatArea) ──────────────────────────

void AiPanel::addUserMessage(const QString &text)
{
    m_chatArea->addMessage(MessageRole::User, text);
}

void AiPanel::addAssistantMessage(const QString &text)
{
    m_chatArea->addMessage(MessageRole::Assistant, text);
}

void AiPanel::appendToLastAssistant(const QString &text)
{
    m_chatArea->appendToLastMessage(text);
}

void AiPanel::flushPendingUpdates()
{
    m_chatArea->flushPendingUpdates();
}

void AiPanel::clearChat()
{
    m_chatArea->clear();
}

void AiPanel::setInputEnabled(bool enabled)
{
    m_inputEdit->setEnabled(enabled);
    m_sendBtn->setEnabled(enabled && !m_inputEdit->text().trimmed().isEmpty());
    m_newConvBtn->setEnabled(enabled);
    m_clearBtn->setEnabled(enabled);
}

void AiPanel::setActionList(const QVector<AiAction> &actions)
{
    m_actionBar->setActions(actions);
}

void AiPanel::clearActionList()
{
    m_actionBar->clearActions();
}

QString AiPanel::lastAssistantContent() const
{
    if (m_chatArea->messageCount() == 0)
        return {};
    ChatBubble *last = m_chatArea->lastBubble();
    if (!last || last->role() != MessageRole::Assistant)
        return {};
    return last->text();
}

bool AiPanel::hasStreamingTarget() const
{
    if (m_chatArea->messageCount() == 0)
        return false;
    ChatBubble *last = m_chatArea->lastBubble();
    return last && last->role() == MessageRole::Assistant && last->text().isEmpty();
}
