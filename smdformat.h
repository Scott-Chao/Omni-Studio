#ifndef SMDFORMAT_H
#define SMDFORMAT_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <QVector>
#include <QPair>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>

// Conversion debug log — outputs to release folder
static void convLog(const QString &msg)
{
    static QFile logFile;
    static QTextStream stream;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        QString logPath = QCoreApplication::applicationDirPath() + QStringLiteral("/conversion_debug.log");
        logFile.setFileName(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            stream.setDevice(&logFile);
            stream << QStringLiteral("=== Conversion Debug Log ===") << Qt::endl;
            stream.flush();
        }
    }
    if (stream.device()) {
        stream << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")) << QStringLiteral(" ") << msg << Qt::endl;
        stream.flush();
    }
}

namespace SmdFormat {

struct Cell {
    QString type;    // "markdown", "cpp", "python"
    QString content;
    bool rendered = false;
    QString output;
};

struct FromMarkdownResult {
    QList<Cell> cells;
    QVector<int> mdLineToCell;
    QVector<int> mdLineToCellLine;
};

struct ToMarkdownResult {
    QString markdown;
    QVector<QPair<int,int>> cellCharRanges;
    QVector<int> cellContentStartLine;
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

    // stripTrailingBlank: only strip when cell was terminated by a delimiter
    // (the trailing blank line is the separator between cells, not user content).
    // For the last cell terminated by EOF, preserve trailing blank lines as-is.
    auto flushCell = [&](bool stripTrailingBlank = true) {
        if (currentType.isEmpty() && currentContent.isEmpty())
            return;
        if (currentType.isEmpty())
            currentType = QStringLiteral("markdown");
        if (stripTrailingBlank && !currentContent.isEmpty() && currentContent.last().trimmed().isEmpty())
            currentContent.removeLast();
        Cell cell;
        cell.type = currentType;
        cell.content = currentContent.join(QLatin1Char('\n'));
        cell.rendered = currentRendered;
        cell.output = currentOutput;
        cells.append(cell);
        currentRendered = false;
        currentOutput.clear();
    };

    for (const QString &line : lines) {
        QRegularExpressionMatch m = DELIM_RE.match(line);
        if (m.hasMatch()) {
            flushCell(true);  // delimiter-terminated: strip separator blank line
            currentType = m.captured(1).toLower();
            currentContent.clear();
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
    flushCell(false);  // EOF-terminated: preserve intentional trailing blank lines
    return cells;
}

inline QString serialize(const QList<Cell> &cells)
{
    QStringList result;
    for (int i = 0; i < cells.size(); ++i) {
        const Cell &cell = cells[i];
        QString header = QStringLiteral("---smd:%1").arg(cell.type);
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
            result.append(QString());
    }
    return result.join(QLatin1Char('\n'));
}

inline QString toMarkdown(const QList<Cell> &cells)
{
    QStringList result;
    for (const Cell &cell : cells) {
        if (cell.type == QStringLiteral("markdown")) {
            result.append(cell.content);
        } else {
            QString lang = (cell.type == QStringLiteral("cpp"))
                ? QStringLiteral("cpp") : QStringLiteral("python");
            result.append(QStringLiteral("```%1").arg(lang));
            result.append(cell.content);
            result.append(QStringLiteral("```"));
        }
    }
    return result.join(QLatin1Char('\n'));
}

// Convert cells to .md text with line mapping.
// Always outputs cell content + fence wrappers; no extra separators added.
// The join('\n') between consecutive parts provides exactly the right spacing
// when cell content already contains any intentional blank lines.
inline ToMarkdownResult toMarkdownWithMapping(const QList<Cell> &cells)
{
    ToMarkdownResult result;
    QStringList parts;
    int currentLine = 0;

    convLog(QStringLiteral("toMD: cells=") + QString::number(cells.size()));

    for (int i = 0; i < cells.size(); ++i) {
        const Cell &cell = cells[i];

        if (cell.type == QStringLiteral("markdown")) {
            result.cellContentStartLine.append(currentLine);
            parts.append(cell.content);
            currentLine += cell.content.count(QLatin1Char('\n')) + 1;
        } else {
            QString lang = (cell.type == QStringLiteral("cpp"))
                ? QStringLiteral("cpp") : QStringLiteral("python");
            parts.append(QStringLiteral("```%1").arg(lang));
            currentLine += 1;
            result.cellContentStartLine.append(currentLine);
            parts.append(cell.content);
            currentLine += cell.content.count(QLatin1Char('\n')) + 1;
            parts.append(QStringLiteral("```"));
            currentLine += 1;
        }
    }

    result.markdown = parts.join(QLatin1Char('\n'));
    convLog(QStringLiteral("toMD result: len=") + QString::number(result.markdown.length())
            + QStringLiteral(" cellStartLines=") + QString::number(result.cellContentStartLine.size()));
    return result;
}

// Detect fenced code blocks and convert to cells.
// All content lines (including blank lines) are preserved as-is in cell content.
// No stripping, no skipping — pure split-by-fence.
inline QList<Cell> fromMarkdown(const QString &markdown)
{
    QList<Cell> cells;
    const QStringList lines = markdown.split(QLatin1Char('\n'));

    QString currentType = QStringLiteral("markdown");
    QStringList currentContent;
    bool inFence = false;
    bool inMermaidBlock = false;
    QString fenceLang;

    auto flush = [&]() {
        Cell cell;
        cell.type = currentType;
        cell.content = currentContent.join(QLatin1Char('\n'));
        if (!cell.content.isEmpty() || !currentContent.isEmpty())
            cells.append(cell);
        currentContent.clear();
    };

    for (const QString &line : lines) {
        static const QRegularExpression fenceStart(R"(^```(\w*)$)");

        if (!inFence && !inMermaidBlock) {
            QRegularExpressionMatch m = fenceStart.match(line);
            if (m.hasMatch() && m.captured(1).toLower() == QStringLiteral("mermaid")) {
                flush();
                inMermaidBlock = true;
                currentContent.append(line);
                continue;
            }
        }

        if (inMermaidBlock && line.trimmed() == QStringLiteral("```")) {
            currentContent.append(line);
            Cell cell;
            cell.type = QStringLiteral("markdown");
            cell.content = currentContent.join(QLatin1Char('\n'));
            cells.append(cell);
            currentContent.clear();
            inMermaidBlock = false;
            continue;
        }

        if (inMermaidBlock) {
            currentContent.append(line);
            continue;
        }

        if (!inFence) {
            QRegularExpressionMatch m = fenceStart.match(line);
            if (m.hasMatch()) {
                flush();
                fenceLang = m.captured(1).toLower();
                if (fenceLang.isEmpty() || fenceLang == QStringLiteral("c"))
                    fenceLang = QStringLiteral("cpp");
                currentType = (fenceLang == QStringLiteral("python") || fenceLang == QStringLiteral("py"))
                    ? QStringLiteral("python") : QStringLiteral("cpp");
                inFence = true;
                continue;
            }
        }

        if (inFence && line.trimmed() == QStringLiteral("```")) {
            Cell cell;
            cell.type = currentType;
            cell.content = currentContent.join(QLatin1Char('\n'));
            cells.append(cell);
            currentContent.clear();
            currentType = QStringLiteral("markdown");
            inFence = false;
            continue;
        }

        currentContent.append(line);
    }
    flush();

    return cells;
}

// fromMarkdown with line-to-cell mapping.
// All content lines preserved as-is — no stripping, no skipping. Pure split-by-fence.
inline FromMarkdownResult fromMarkdownWithMapping(const QString &markdown)
{
    FromMarkdownResult result;
    const QStringList lines = markdown.split(QLatin1Char('\n'));

    QString currentType = QStringLiteral("markdown");
    QStringList currentContent;
    bool inFence = false;
    bool inMermaidBlock = false;
    QString fenceLang;
    int currentCellIndex = -1;
    int currentCellLine = 0;

    auto flushCell = [&]() {
        Cell cell;
        cell.type = currentType;
        cell.content = currentContent.join(QLatin1Char('\n'));
        convLog(QStringLiteral("flush: type=") + cell.type
                + QStringLiteral(" len=") + QString::number(cell.content.length())
                + QStringLiteral(" ncell=") + QString::number(result.cells.size()));
        if (!cell.content.isEmpty() || !currentContent.isEmpty())
            result.cells.append(cell);
        currentContent.clear();
        currentCellIndex = -1;
        currentCellLine = 0;
    };

    auto mapLine = [&](int cellIdx, int lineWithinCell) {
        result.mdLineToCell.append(cellIdx);
        result.mdLineToCellLine.append(lineWithinCell);
    };

    convLog(QStringLiteral("fromMD: input lines=") + QString::number(lines.size()));

    for (const QString &line : lines) {
        static const QRegularExpression fenceStart(R"(^```(\w*)$)");

        // Mermaid block: start
        if (!inFence && !inMermaidBlock) {
            QRegularExpressionMatch m = fenceStart.match(line);
            if (m.hasMatch() && m.captured(1).toLower() == QStringLiteral("mermaid")) {
                flushCell();
                inMermaidBlock = true;
                currentCellIndex = result.cells.size();
                currentCellLine = 0;
                currentContent.append(line);
                mapLine(-1, -1);
                continue;
            }
        }

        // Mermaid block: end
        if (inMermaidBlock && line.trimmed() == QStringLiteral("```")) {
            currentContent.append(line);
            mapLine(-1, -1);
            Cell cell;
            cell.type = QStringLiteral("markdown");
            cell.content = currentContent.join(QLatin1Char('\n'));
            result.cells.append(cell);
            currentContent.clear();
            inMermaidBlock = false;
            currentCellIndex = -1;
            currentCellLine = 0;
            continue;
        }

        // Mermaid block: accumulate
        if (inMermaidBlock) {
            currentContent.append(line);
            mapLine(currentCellIndex, currentCellLine);
            ++currentCellLine;
            continue;
        }

        // Regular fence: start
        if (!inFence) {
            QRegularExpressionMatch m = fenceStart.match(line);
            if (m.hasMatch()) {
                flushCell();
                fenceLang = m.captured(1).toLower();
                if (fenceLang.isEmpty() || fenceLang == QStringLiteral("c"))
                    fenceLang = QStringLiteral("cpp");
                currentType = (fenceLang == QStringLiteral("python") || fenceLang == QStringLiteral("py"))
                    ? QStringLiteral("python") : QStringLiteral("cpp");
                inFence = true;
                mapLine(-1, -1);
                currentCellIndex = result.cells.size();
                currentCellLine = 0;
                continue;
            }
        }

        // Regular fence: end
        if (inFence && line.trimmed() == QStringLiteral("```")) {
            Cell cell;
            cell.type = currentType;
            cell.content = currentContent.join(QLatin1Char('\n'));
            result.cells.append(cell);
            currentContent.clear();
            currentType = QStringLiteral("markdown");
            inFence = false;
            mapLine(-1, -1);
            currentCellIndex = -1;
            currentCellLine = 0;
            continue;
        }

        // Accumulate content
        if (!inFence && currentCellIndex < 0)
            currentCellIndex = result.cells.size();
        currentContent.append(line);
        mapLine(currentCellIndex, currentCellLine);
        ++currentCellLine;
    }

    flushCell();

    convLog(QStringLiteral("fromMD result: cells=") + QString::number(result.cells.size()));
    return result;
}

} // namespace SmdFormat

#endif // SMDFORMAT_H
