#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class QLineEdit;
class QPushButton;
class QCheckBox;

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);

    QString username() const;
    QString password() const;
    bool isAutoLoginEnabled() const;
    void setAutoLoginEnabled(bool enabled);

private:
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QPushButton *m_loginBtn;
    QPushButton *m_skipBtn;
    QCheckBox *m_autoLoginCheck;
};

#endif // LOGINDIALOG_H
