#ifndef CHATBUBBLE_H
#define CHATBUBBLE_H

#include <QWidget>
#include <QString>

class QTextBrowser;
class QLabel;

class ChatBubble : public QWidget
{
    Q_OBJECT

public:
    enum Role { User, Assistant };

    ChatBubble(Role role, const QString &text, QWidget *parent = nullptr);

    void setText(const QString &text);
    void appendText(const QString &text);
    Role role() const { return m_role; }
    QString text() const { return m_text; }

    static QString markdownToHtml(const QString &md);

private:
    void updateContent();
    QString messageStyleSheet() const;

    Role m_role;
    QString m_text;
    QTextBrowser *m_browser;
    QLabel *m_roleLabel;
};

#endif // CHATBUBBLE_H
