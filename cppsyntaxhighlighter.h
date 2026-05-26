#ifndef CPPSYNTAXHIGHLIGHTER_H
#define CPPSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>
#include <QMap>
#include <QList>

#include "completionprovider.h"

class CppSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit CppSyntaxHighlighter(QTextDocument *parent = nullptr);

    void setSemanticTokens(const QList<SemanticToken> &tokens);
    void clearSemanticTokens();

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureGroup = 0; // 0 = entire match, 1+ = specific capture group
    };
    QVector<HighlightingRule> m_rules;

    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_preprocessorFormat;
    QTextCharFormat m_typeFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_includeHeaderFormat;
    QTextCharFormat m_parameterFormat;
    QTextCharFormat m_singleLineCommentFormat;
    QTextCharFormat m_multiLineCommentFormat;

    QRegularExpression m_commentStartExpr;
    QRegularExpression m_commentEndExpr;

    // Semantic tokens indexed by block number (0-based)
    QMap<int, QList<SemanticToken>> m_semanticTokens;

    QTextCharFormat formatForTokenType(const QString &type) const;
};

#endif // CPPSYNTAXHIGHLIGHTER_H
