#ifndef PYTHONSYNTAXHIGHLIGHTER_H
#define PYTHONSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>
#include <QMap>
#include <QList>

#include "completionprovider.h"

class PythonSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit PythonSyntaxHighlighter(QTextDocument *parent = nullptr);

    void setSemanticTokens(const QList<SemanticToken> &tokens);
    void clearSemanticTokens();

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

    QColor m_bracketColors[3];
    QColor m_unpairedBracketColor;

    QRegularExpression m_tripleSingleStart;
    QRegularExpression m_tripleSingleEnd;
    QRegularExpression m_tripleDoubleStart;
    QRegularExpression m_tripleDoubleEnd;

    void initFormats();
    QTextCharFormat formatForTokenType(const QString &type) const;
    void highlightBrackets(const QString &text);

    QMap<int, QList<SemanticToken>> m_semanticTokens;
};

#endif // PYTHONSYNTAXHIGHLIGHTER_H
