#include "logindialog.h"
#include "config/configmanager.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("登录 OpenJudge"));
    setFixedSize(ConfigManager::instance().loginDialogWidth(),
                 ConfigManager::instance().loginDialogHeight());

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("请输入 OpenJudge 账号信息"));
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(ConfigManager::instance().loginDialogTitleFontSize());
    titleLabel->setFont(titleFont);

    auto *formLayout = new QFormLayout;
    formLayout->setSpacing(8);

    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText(QStringLiteral("用户名"));
    formLayout->addRow(QStringLiteral("账号:"), m_usernameEdit);

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setPlaceholderText(QStringLiteral("密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(QStringLiteral("密码:"), m_passwordEdit);

    m_autoLoginCheck = new QCheckBox(QStringLiteral("自动登录"));
    m_autoLoginCheck->setChecked(false);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(12);

    m_skipBtn = new QPushButton(QStringLiteral("跳过登录"));
    m_loginBtn = new QPushButton(QStringLiteral("登录"));
    m_loginBtn->setDefault(true);

    btnLayout->addWidget(m_skipBtn);
    btnLayout->addWidget(m_loginBtn);

    mainLayout->addWidget(titleLabel);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_autoLoginCheck, 0, Qt::AlignCenter);
    mainLayout->addLayout(btnLayout);
    mainLayout->addStretch();

    connect(m_loginBtn, &QPushButton::clicked, this, [this]() {
        if (m_usernameEdit->text().isEmpty()) {
            m_usernameEdit->setFocus();
            return;
        }
        accept();
    });

    connect(m_skipBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QString LoginDialog::username() const
{
    return m_usernameEdit->text().trimmed();
}

QString LoginDialog::password() const
{
    return m_passwordEdit->text();
}

bool LoginDialog::isAutoLoginEnabled() const
{
    return m_autoLoginCheck->isChecked();
}

void LoginDialog::setAutoLoginEnabled(bool enabled)
{
    m_autoLoginCheck->setChecked(enabled);
}
