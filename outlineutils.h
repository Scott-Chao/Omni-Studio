#ifndef OUTLINEUTILS_H
#define OUTLINEUTILS_H

#include <QString>
#include <QVector>
#include <QRegularExpression>
#include <QStringList>

struct HeadingItem {
    QString text;
    int level = 1;       // 1-6
    int lineNumber = 0;  // 1-based
};

inline QVector<HeadingItem> extractHeadingsFromContent(const QString &content)
{
    QVector<HeadingItem> headings;

    static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})\\s+(.+)$"),
                                              QRegularExpression::MultilineOption);

    const QStringList lines = content.split(QLatin1Char('\n'));
    bool inCodeBlock = false;

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        // Toggle code block state
        if (line.trimmed().startsWith(QStringLiteral("```"))) {
            inCodeBlock = !inCodeBlock;
            continue;
        }
        if (inCodeBlock)
            continue;

        QRegularExpressionMatch match = headingRe.match(line);
        if (match.hasMatch()) {
            HeadingItem item;
            item.level = match.captured(1).length();
            item.text = match.captured(2).trimmed();
            item.lineNumber = i + 1; // 1-based line number
            headings.append(item);
        }
    }

    return headings;
}

#endif // OUTLINEUTILS_H
