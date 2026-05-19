#include "aipanel.h"
#include "chatarea.h"
#include "actionbar.h"
#include "errorlistpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>

// ── Shared tab button style ───────────────────────────────────────────

static const char *kInactiveTabStyle = R"(
    QPushButton {
        background: transparent;
        color: #777;
        border: none;
        border-bottom: 2px solid transparent;
        font-size: 12px;
        font-weight: normal;
        padding: 0 10px;
    }
    QPushButton:hover {
        color: #aaa;
    }
)";

static const char *kActiveTabStyle = R"(
    QPushButton {
        background: transparent;
        color: #cccccc;
        border: none;
        border-bottom: 2px solid #0078d4;
        font-size: 12px;
        font-weight: bold;
        padding: 0 10px;
    }
)";

// ═══════════════════════════════════════════════════════════════════════

AiPanel::AiPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(DefaultWidth);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Title bar with tab buttons ──
    auto *titleBar = new QWidget(this);
    titleBar->setFixedHeight(36);
    titleBar->setStyleSheet("background-color: #252525;");

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(4, 0, 6, 0);
    titleLayout->setSpacing(2);

    m_aiTabBtn = new QPushButton(tr("AI 助手"));
    m_aiTabBtn->setFixedHeight(32);
    m_aiTabBtn->setCursor(Qt::PointingHandCursor);

    m_errorTabBtn = new QPushButton(tr("错题本"));
    m_errorTabBtn->setFixedHeight(32);
    m_errorTabBtn->setCursor(Qt::PointingHandCursor);

    m_clearBtn = new QPushButton(tr("清空对话"));
    m_clearBtn->setFixedHeight(24);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  border: 1px solid #555;"
        "  border-radius: 4px;"
        "  color: #999;"
        "  font-size: 11px;"
        "  padding: 0 8px;"
        "}"
        "QPushButton:hover {"
        "  color: #fff;"
        "  border-color: #888;"
        "}"
    );

    titleLayout->addWidget(m_aiTabBtn);
    titleLayout->addWidget(m_errorTabBtn);
    titleLayout->addStretch();
    titleLayout->addWidget(m_clearBtn);

    // ── Action bar (only shown on chat tab) ──
    m_actionBar = new ActionBar(this);

    // ── Stacked widget: chat area (0) | error list (1) ──
    m_stackedWidget = new QStackedWidget(this);

    m_chatArea = new ChatArea(this);
    m_errorListPanel = new ErrorListPanel(this);

    m_stackedWidget->addWidget(m_chatArea);       // index 0 = ChatTab
    m_stackedWidget->addWidget(m_errorListPanel);  // index 1 = ErrorTab

    // ── Input bar (only shown on chat tab) ──
    m_inputBar = new QWidget(this);
    m_inputBar->setStyleSheet("background-color: #252525;");
    auto *inputLayout = new QHBoxLayout(m_inputBar);
    inputLayout->setContentsMargins(6, 6, 6, 6);
    inputLayout->setSpacing(6);

    m_inputEdit = new QLineEdit;
    m_inputEdit->setPlaceholderText(tr("输入消息..."));
    m_inputEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #3c3c3c;"
        "  color: #cccccc;"
        "  border: 1px solid #555555;"
        "  border-radius: 4px;"
        "  padding: 6px 8px;"
        "  font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #0078d4;"
        "}"
    );

    m_sendBtn = new QPushButton(tr("发送"));
    m_sendBtn->setFixedSize(50, 28);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setEnabled(false);
    m_sendBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #0078d4;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #1a8ad4;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #555555;"
        "  color: #888888;"
        "}"
    );

    inputLayout->addWidget(m_inputEdit, 1);
    inputLayout->addWidget(m_sendBtn);

    // ── Assemble main layout ──
    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(m_actionBar);
    mainLayout->addWidget(m_stackedWidget, 1);
    mainLayout->addWidget(m_inputBar);

    // ── Start on chat tab ──
    updateTabButtonStyle();
    m_stackedWidget->setCurrentIndex(ChatTab);

    // ── Connections ──
    connect(m_aiTabBtn, &QPushButton::clicked, this, [this]() { onTabSwitch(ChatTab); });
    connect(m_errorTabBtn, &QPushButton::clicked, this, [this]() { onTabSwitch(ErrorTab); });

    connect(m_actionBar, &ActionBar::actionTriggered, this, [this](AiAction action) {
        emit actionTriggered(static_cast<int>(action));
    });

    connect(m_inputEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_sendBtn->setEnabled(!text.trimmed().isEmpty());
    });
    connect(m_inputEdit, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_inputEdit->text().trimmed();
        if (!text.isEmpty()) {
            emit sendMessage(text);
            m_inputEdit->clear();
        }
    });
    connect(m_sendBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_inputEdit->text().trimmed();
        if (!text.isEmpty()) {
            emit sendMessage(text);
            m_inputEdit->clear();
        }
    });
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        clearChat();
        emit clearRequested();
    });

    // Forward error list item click
    connect(m_errorListPanel, &ErrorListPanel::errorClicked,
            this, &AiPanel::errorSelected);
}

void AiPanel::onTabSwitch(int index)
{
    if (index == m_currentTab)
        return;

    m_currentTab = index;
    m_stackedWidget->setCurrentIndex(index);
    updateTabButtonStyle();

    // Show/hide chat-specific UI
    bool isChat = (index == ChatTab);
    m_actionBar->setVisible(isChat);
    m_inputBar->setVisible(isChat);
    m_clearBtn->setVisible(isChat);

    // Refresh error list when switching to error tab
    if (!isChat) {
        m_errorListPanel->loadRecords();
    }
}

void AiPanel::updateTabButtonStyle()
{
    m_aiTabBtn->setStyleSheet(m_currentTab == ChatTab ? kActiveTabStyle : kInactiveTabStyle);
    m_errorTabBtn->setStyleSheet(m_currentTab == ErrorTab ? kActiveTabStyle : kInactiveTabStyle);
}

void AiPanel::setCurrentTab(int index)
{
    onTabSwitch(index);
}

// ── Chat operations (delegated to ChatArea) ──────────────────────────

void AiPanel::addUserMessage(const QString &text)
{
    m_chatArea->addMessage(ChatBubble::User, text);
}

void AiPanel::addAssistantMessage(const QString &text)
{
    m_chatArea->addMessage(ChatBubble::Assistant, text);
}

void AiPanel::appendToLastAssistant(const QString &text)
{
    m_chatArea->appendToLastMessage(text);
}

void AiPanel::clearChat()
{
    m_chatArea->clear();
}

void AiPanel::setInputEnabled(bool enabled)
{
    m_inputEdit->setEnabled(enabled);
    m_sendBtn->setEnabled(enabled && !m_inputEdit->text().trimmed().isEmpty());
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
    if (!last || last->role() != ChatBubble::Assistant)
        return {};
    return last->text();
}

bool AiPanel::hasStreamingTarget() const
{
    if (m_chatArea->messageCount() == 0)
        return false;
    ChatBubble *last = m_chatArea->lastBubble();
    return last && last->role() == ChatBubble::Assistant && last->text().isEmpty();
}
