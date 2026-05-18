#include "aipanel.h"
#include "chatarea.h"
#include "actionbar.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

AiPanel::AiPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(DefaultWidth);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Title bar ──
    auto *titleBar = new QWidget(this);
    titleBar->setFixedHeight(36);
    titleBar->setStyleSheet("background-color: #252525;");

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 6, 0);

    auto *titleLabel = new QLabel(tr("AI 助手"));
    titleLabel->setStyleSheet("color: #cccccc; font-size: 12px; font-weight: bold;");

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

    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_clearBtn);

    // ── Action bar ──
    m_actionBar = new ActionBar(this);

    // ── Chat area ──
    m_chatArea = new ChatArea(this);

    // ── Input bar ──
    auto *inputBar = new QWidget(this);
    inputBar->setStyleSheet("background-color: #252525;");
    auto *inputLayout = new QHBoxLayout(inputBar);
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

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(m_actionBar);
    mainLayout->addWidget(m_chatArea, 1);
    mainLayout->addWidget(inputBar);

    // ── Connections ──
    connect(m_actionBar, &ActionBar::actionTriggered, this, [this](AiAction action) {
        emit actionTriggered(static_cast<int>(action));
    });

    connect(m_inputEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_sendBtn->setEnabled(!text.trimmed().isEmpty());
    });
    connect(m_inputEdit, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_inputEdit->text().trimmed();
        if (!text.isEmpty()) {
            addUserMessage(text);
            emit sendMessage(text);
            m_inputEdit->clear();
        }
    });
    connect(m_sendBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_inputEdit->text().trimmed();
        if (!text.isEmpty()) {
            addUserMessage(text);
            emit sendMessage(text);
            m_inputEdit->clear();
        }
    });
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        clearChat();
        emit clearRequested();
    });
}

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
