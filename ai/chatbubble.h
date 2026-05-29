#ifndef CHATBUBBLE_H
#define CHATBUBBLE_H

#include <QWidget>
#include <QString>
#include <QColor>

class QTextBrowser;
class QLabel;
class QResizeEvent;
class QTimer;

#include "aiproviders.h"

class ChatBubble : public QWidget
{
    Q_OBJECT

public:
    ChatBubble(MessageRole role, const QString &text, QWidget *parent = nullptr);

    void setText(const QString &text);
    void appendText(const QString &text);
    void flushUpdate();
    MessageRole role() const { return m_role; }
    QString text() const { return m_text; }

    static QString markdownToHtml(const QString &md, const QColor &textColor = QColor("#D4D4D4"),
                                  const QColor &codeBg = QColor("#3C3C3C"),
                                  const QColor &codeFg = QColor("#CE9178"),
                                  const QColor &linkColor = QColor("#4DA3FF"),
                                  const QColor &selectionBg = QColor("#264F78"),
                                  const QColor &headingColor = QColor("#FFFFFF"));

private:
    void updateContent();
    void updateBrowserHeight();
    void refreshStyle();
    void resizeEvent(QResizeEvent *event) override;
    QString messageStyleSheet() const;

    // Incremental streaming helpers
    bool isStructuralDelta(const QString &delta) const;
    QString processSimpleDelta(const QString &delta, const QColor &textColor,
                                const QColor &codeBg, const QColor &codeFg,
                                const QColor &linkColor) const;

    MessageRole m_role;
    QString m_text;
    QTextBrowser *m_browser;
    QLabel *m_roleLabel;
    QTimer *m_updateTimer;

    // Incremental HTML cache — avoids re-running markdownToHtml on full text
    // on every streaming tick. m_accumulatedHtml grows by converting only the
    // delta chunk; full markdownToHtml rebuild happens only on structural
    // boundaries (code blocks, headings, lists) and theme changes.
    QString m_accumulatedHtml;
    int m_lastProcessedLength = 0;
    bool m_fullRebuildNeeded = false;
    bool m_inCodeBlock = false;

    static constexpr int UPDATE_INTERVAL_MS = 80;
};

#endif // CHATBUBBLE_H
