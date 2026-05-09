#ifndef PYTHONSYNTAXHIGHLIGHTER_H
#define PYTHONSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>

class PythonSyntaxHighlighter : public QSyntaxHighlighter
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
    QTextCharFormat m_builtinFormat;
    QTextCharFormat m_decoratorFormat;
    QTextCharFormat m_selfFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_tripleFormat;

    QRegularExpression m_tripleSingleStart;
    QRegularExpression m_tripleSingleEnd;
    QRegularExpression m_tripleDoubleStart;
    QRegularExpression m_tripleDoubleEnd;
};

#endif // PYTHONSYNTAXHIGHLIGHTER_H
