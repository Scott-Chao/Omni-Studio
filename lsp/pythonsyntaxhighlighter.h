#ifndef PYTHONSYNTAXHIGHLIGHTER_H
#define PYTHONSYNTAXHIGHLIGHTER_H

#include <QRegularExpression>
#include <QVector>

#include "brackethighlighter.h"

class PythonSyntaxHighlighter : public BracketHighlighter
{
    Q_OBJECT

public:
    explicit PythonSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> m_rules;

    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_controlKeywordFormat;
    QTextCharFormat m_builtinFormat;
    QTextCharFormat m_decoratorFormat;
    QTextCharFormat m_selfFormat;
    QTextCharFormat m_parameterFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_tripleFormat;

    QRegularExpression m_tripleSingleStart;
    QRegularExpression m_tripleSingleEnd;
    QRegularExpression m_tripleDoubleStart;
    QRegularExpression m_tripleDoubleEnd;

    void initFormats();
    QTextCharFormat formatForTokenType(const QString &type) const;
};

#endif // PYTHONSYNTAXHIGHLIGHTER_H
