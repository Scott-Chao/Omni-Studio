#ifndef CPPSYNTAXHIGHLIGHTER_H
#define CPPSYNTAXHIGHLIGHTER_H

#include <QRegularExpression>
#include <QVector>

#include "brackethighlighter.h"

class CppSyntaxHighlighter : public BracketHighlighter
{
    Q_OBJECT

public:
    explicit CppSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureGroup = 0;
    };
    QVector<HighlightingRule> m_rules;

    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_controlKeywordFormat;
    QTextCharFormat m_primitiveTypeFormat;
    QTextCharFormat m_preprocessorFormat;
    QTextCharFormat m_typeFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_includeHeaderFormat;
    QTextCharFormat m_parameterFormat;
    QTextCharFormat m_operatorFormat;
    QTextCharFormat m_singleLineCommentFormat;
    QTextCharFormat m_multiLineCommentFormat;

    QRegularExpression m_commentStartExpr;
    QRegularExpression m_commentEndExpr;

    QTextCharFormat formatForTokenType(const QString &type) const;
    void initFormats();
};

#endif // CPPSYNTAXHIGHLIGHTER_H
