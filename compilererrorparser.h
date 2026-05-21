#ifndef COMPILERERRORPARSER_H
#define COMPILERERRORPARSER_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include "smddiagnostic.h"

namespace CompilerErrorParser {

inline QList<SmdDiagnostic> parseCompileErrors(const QString &stderrText, int blockIndex)
{
    QList<SmdDiagnostic> result;
    const QStringList lines = stderrText.split(QLatin1Char('\n'));

    // g++:  file:line:col: error/warning/note: message
    static const QRegularExpression gccRe(
        QStringLiteral("^(.+?):(\\d+):(\\d+):\\s*(error|warning|note):\\s*(.*)$"));

    // MSVC: file(line,col): error/warning Cxxxx: message
    // or:   file(line): error/warning Cxxxx: message (no column)
    static const QRegularExpression msvcRe(
        QStringLiteral("^(.+?)\\((\\d+)(?:,(\\d+))?\\):\\s*(error|warning)\\s+(C\\d+):\\s*(.*)$"));

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;

        SmdDiagnostic d;
        d.cellIndex = blockIndex;

        QRegularExpressionMatch m = gccRe.match(trimmed);
        if (m.hasMatch()) {
            QString sevStr = m.captured(4).toLower();
            if (sevStr == QLatin1String("note"))
                continue; // skip supplementary notes
            d.startLine = m.captured(2).toInt() - 1;
            d.startCol  = m.captured(3).toInt() - 1;
            d.endLine   = d.startLine;
            d.endCol    = d.startCol + 1;
            d.severity  = (sevStr == QLatin1String("error")) ? 1 : 2;
            d.message   = m.captured(5).trimmed();
            result.append(d);
            continue;
        }

        m = msvcRe.match(trimmed);
        if (m.hasMatch()) {
            QString sevStr = m.captured(4).toLower();
            d.startLine = m.captured(2).toInt() - 1;
            d.startCol  = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt() - 1;
            d.endLine   = d.startLine;
            d.endCol    = d.startCol + 1;
            d.severity  = (sevStr == QLatin1String("error")) ? 1 : 2;
            d.message   = QStringLiteral("%1: %2")
                              .arg(m.captured(5), m.captured(6).trimmed());
            result.append(d);
        }
    }

    return result;
}

inline QList<SmdDiagnostic> parsePythonTraceback(const QString &stderrText, int blockIndex)
{
    QList<SmdDiagnostic> result;
    const QStringList lines = stderrText.split(QLatin1Char('\n'));

    // Match:   File "path", line N, in <scope>
    static const QRegularExpression fileRe(
        QStringLiteral("^\\s*File\\s+\"(.+?)\",\\s*line\\s+(\\d+)"));

    int lastFileLine = -1;
    QString lastFilePath;
    QString errorMessage;

    for (const QString &line : lines) {
        QRegularExpressionMatch m = fileRe.match(line);
        if (m.hasMatch()) {
            lastFilePath = m.captured(1);
            lastFileLine = m.captured(2).toInt() - 1;
            continue;
        }

        // After traceback lines, the error type + message appears
        // e.g. "ZeroDivisionError: division by zero"
        // e.g. "SyntaxError: invalid syntax"
        // Skip stack-frame lines that start with whitespace (source code context)
        if (lastFileLine >= 0 && !line.startsWith(QLatin1Char(' '))
            && !line.trimmed().isEmpty()
            && !line.startsWith(QLatin1String("Traceback"))
            && !line.startsWith(QLatin1String("  File "))) {
            errorMessage = line.trimmed();
        }
    }

    // If no structured line info was found, use the first meaningful line
    if (lastFileLine < 0) {
        for (const QString &line : lines) {
            QString t = line.trimmed();
            if (!t.isEmpty() && !t.startsWith(QLatin1String("Traceback"))) {
                errorMessage = t;
                break;
            }
        }
    }

    if (lastFileLine >= 0 || !errorMessage.isEmpty()) {
        SmdDiagnostic d;
        d.cellIndex = blockIndex;
        d.startLine = lastFileLine >= 0 ? lastFileLine : 0;
        d.startCol  = 0;
        d.endLine   = d.startLine;
        d.endCol    = 1;
        d.severity  = 1; // runtime errors are always severity 1
        d.message   = errorMessage;
        result.append(d);
    }

    return result;
}

} // namespace CompilerErrorParser

#endif // COMPILERERRORPARSER_H
