#ifndef SMDFORMAT_H
#define SMDFORMAT_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QRegularExpression>

namespace SmdFormat {

struct Cell {
    QString type;    // "markdown", "cpp", "python"
    QString content;
};

static const QRegularExpression DELIM_RE(QStringLiteral(R"(^---smd:(\w+)$)"));

inline QList<Cell> parse(const QString &text)
{
    QList<Cell> cells;
    const QStringList lines = text.split(QLatin1Char('\n'));

    QString currentType;
    QStringList currentContent;

    auto flushCell = [&]() {
        if (currentType.isEmpty() && currentContent.isEmpty())
            return;
        if (currentType.isEmpty())
            currentType = QStringLiteral("markdown");
        // Trim one trailing blank line to strip cell separator ambiguity
        if (!currentContent.isEmpty() && currentContent.last().trimmed().isEmpty())
            currentContent.removeLast();
        Cell cell;
        cell.type = currentType;
        cell.content = currentContent.join(QLatin1Char('\n'));
        cells.append(cell);
    };

    for (const QString &line : lines) {
        QRegularExpressionMatch m = DELIM_RE.match(line);
        if (m.hasMatch()) {
            flushCell();
            currentType = m.captured(1).toLower();
            currentContent.clear();
        } else {
            currentContent.append(line);
        }
    }
    flushCell();
    return cells;
}

inline QString serialize(const QList<Cell> &cells)
{
    QStringList result;
    for (int i = 0; i < cells.size(); ++i) {
        const Cell &cell = cells[i];
        result.append(QStringLiteral("---smd:%1").arg(cell.type));
        if (!cell.content.isEmpty())
            result.append(cell.content);
        if (i < cells.size() - 1)
            result.append(QString()); // blank line between cells
    }
    return result.join(QLatin1Char('\n'));
}

// Stub: convert Smd document to .md text
inline QString toMarkdown(const QList<Cell> &cells)
{
    QStringList result;
    for (const Cell &cell : cells) {
        if (cell.type == QStringLiteral("markdown")) {
            if (!cell.content.isEmpty())
                result.append(cell.content);
        } else {
            QString lang = (cell.type == QStringLiteral("cpp"))
                ? QStringLiteral("cpp") : QStringLiteral("python");
            result.append(QStringLiteral("```%1").arg(lang));
            if (!cell.content.isEmpty())
                result.append(cell.content);
            result.append(QStringLiteral("```"));
        }
        result.append(QString());
    }
    return result.join(QLatin1Char('\n'));
}

// Stub: heuristic reverse — detect fenced code blocks and convert to cells
inline QList<Cell> fromMarkdown(const QString &markdown)
{
    QList<Cell> cells;
    const QStringList lines = markdown.split(QLatin1Char('\n'));

    QString currentType = QStringLiteral("markdown");
    QStringList currentContent;
    bool inFence = false;
    QString fenceLang;

    for (const QString &line : lines) {
        static const QRegularExpression fenceStart(R"(^```(\w*)$)");
        QRegularExpressionMatch m = fenceStart.match(line);
        if (!inFence && m.hasMatch()) {
            // Flush current markdown cell
            if (!currentContent.isEmpty()) {
                Cell cell;
                cell.type = currentType;
                while (!currentContent.isEmpty() && currentContent.last().trimmed().isEmpty())
                    currentContent.removeLast();
                cell.content = currentContent.join(QLatin1Char('\n'));
                if (!cell.content.isEmpty())
                    cells.append(cell);
                currentContent.clear();
            }
            fenceLang = m.captured(1).toLower();
            if (fenceLang.isEmpty() || fenceLang == QStringLiteral("c"))
                fenceLang = QStringLiteral("cpp");
            currentType = (fenceLang == QStringLiteral("python") || fenceLang == QStringLiteral("py"))
                ? QStringLiteral("python") : QStringLiteral("cpp");
            inFence = true;
        } else if (inFence && line.trimmed() == QStringLiteral("```")) {
            // End of fence
            Cell cell;
            cell.type = currentType;
            while (!currentContent.isEmpty() && currentContent.last().trimmed().isEmpty())
                currentContent.removeLast();
            cell.content = currentContent.join(QLatin1Char('\n'));
            if (!cell.content.isEmpty())
                cells.append(cell);
            currentContent.clear();
            currentType = QStringLiteral("markdown");
            inFence = false;
        } else {
            currentContent.append(line);
        }
    }
    // Flush remaining content as markdown
    if (!currentContent.isEmpty()) {
        Cell cell;
        cell.type = QStringLiteral("markdown");
        while (!currentContent.isEmpty() && currentContent.last().trimmed().isEmpty())
            currentContent.removeLast();
        cell.content = currentContent.join(QLatin1Char('\n'));
        if (!cell.content.isEmpty())
            cells.append(cell);
    }
    return cells;
}

} // namespace SmdFormat

#endif // SMDFORMAT_H
