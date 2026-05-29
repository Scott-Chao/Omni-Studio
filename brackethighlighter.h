#ifndef BRACKETHIGHLIGHTER_H
#define BRACKETHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QColor>
#include <QMap>
#include <QList>

#include "completionprovider.h"

class BracketHighlighter : public QSyntaxHighlighter
{
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

    void setSemanticTokens(const QList<SemanticToken> &tokens);
    void clearSemanticTokens();

protected:
    void highlightBrackets(const QString &text,
                           const QColor &commentFg,
                           const QColor &stringFg,
                           const QColor &preprocessorFg = QColor(),
                           bool checkPreprocessor = false);

    QMap<int, QList<SemanticToken>> m_semanticTokens;

    QColor m_bracketColors[3] = {
        QColor("#FFD700"),
        QColor("#DA70D6"),
        QColor("#179FFF")
    };
    QColor m_unpairedBracketColor = QColor("#FF0000");
};

#endif // BRACKETHIGHLIGHTER_H
