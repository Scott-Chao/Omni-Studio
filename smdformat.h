#ifndef SMDFORMAT_H
#define SMDFORMAT_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>

namespace SmdFormat {

struct Cell {
    QString type;    // "markdown", "cpp", "python"
    QString content;
    bool rendered = false;   // MD cell render state
    QString output;          // raw stdout/stderr for code cells
};

static const QRegularExpression DELIM_RE(QStringLiteral(R"(^---smd:(\w+)\s*(\{.*\})?$)"));

inline QList<Cell> parse(const QString &text)
{
    QList<Cell> cells;
    const QStringList lines = text.split(QLatin1Char('\n'));

    QString currentType;
    QStringList currentContent;
    bool currentRendered = false;
    QString currentOutput;

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
        cell.rendered = currentRendered;
        cell.output = currentOutput;
        cells.append(cell);
        // Reset metadata for next cell
        currentRendered = false;
        currentOutput.clear();
    };

    for (const QString &line : lines) {
        QRegularExpressionMatch m = DELIM_RE.match(line);
        if (m.hasMatch()) {
            flushCell();
            currentType = m.captured(1).toLower();
            currentContent.clear();
            // Parse optional JSON metadata
            QString metaJson = m.captured(2).trimmed();
            if (!metaJson.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(metaJson.toUtf8());
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    currentRendered = obj.value(QStringLiteral("rendered")).toBool(false);
                    if (obj.contains(QStringLiteral("output"))) {
                        QString b64 = obj.value(QStringLiteral("output")).toString();
                        currentOutput = QString::fromUtf8(
                            QByteArray::fromBase64(b64.toLatin1()));
                    }
                }
            }
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
        QString header = QStringLiteral("---smd:%1").arg(cell.type);
        // Build optional metadata JSON
        QJsonObject meta;
        if (cell.rendered)
            meta.insert(QStringLiteral("rendered"), true);
        if (!cell.output.isEmpty()) {
            QString b64 = QString::fromLatin1(cell.output.toUtf8().toBase64());
            meta.insert(QStringLiteral("output"), b64);
        }
        if (!meta.isEmpty()) {
            QJsonDocument doc(meta);
            header += QLatin1Char(' ') + QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        }
        result.append(header);
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
