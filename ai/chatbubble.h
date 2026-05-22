#ifndef CHATBUBBLE_H
#define CHATBUBBLE_H

#include <QWidget>
#include <QString>
#include <QColor>

class QTextBrowser;
class QLabel;
class QResizeEvent;

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

    static QString markdownToHtml(const QString &md, const QColor &textColor = QColor("#D4D4D4"),
                                  const QColor &codeBg = QColor("#3C3C3C"),
                                  const QColor &codeFg = QColor("#CE9178"),
                                  const QColor &linkColor = QColor("#4DA3FF"),
                                  const QColor &selectionBg = QColor("#264F78"),
                                  const QColor &headingColor = QColor("#FFFFFF"));

private:
    void updateContent();
    void refreshStyle();
    void resizeEvent(QResizeEvent *event) override;
    QString messageStyleSheet() const;

    Role m_role;
    QString m_text;
    QTextBrowser *m_browser;
    QLabel *m_roleLabel;
};

#endif // CHATBUBBLE_H
