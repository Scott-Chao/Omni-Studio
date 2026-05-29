#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <QString>

namespace StringUtils {

// Normalize line-endings and lone surrogates for Python consumption.
// Qt strings may carry \r\n (Windows) or lone surrogates (UTF-16 artifacts);
// Python's compile() chokes on both.
inline QString sanitizeForPython(const QString &s)
{
    QString out = s;
    out.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    out.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    for (int i = 0; i < out.size(); ++i) {
        QChar ch = out.at(i);
        if (ch.isLowSurrogate() && (i == 0 || !out.at(i - 1).isHighSurrogate()))
            out[i] = QChar(QChar::ReplacementCharacter);
        else if (ch.isHighSurrogate()
                 && (i + 1 >= out.size() || !out.at(i + 1).isLowSurrogate()))
            out[i] = QChar(QChar::ReplacementCharacter);
    }
    // UTF-8 round-trip: correctly encodes valid surrogate pairs and
    // replaces any remaining lone surrogates with U+FFFD, ensuring
    // the Python json parser never sees bare surrogates.
    out = QString::fromUtf8(out.toUtf8());
    return out;
}

// Convert LSP CompletionItemKind integer code to human-readable string.
// See LSP spec for the full list (1-25).
inline QString completionKindToString(int kind)
{
    switch (kind) {
    case 1:  return QStringLiteral("Text");
    case 2:  return QStringLiteral("Method");
    case 3:  return QStringLiteral("Function");
    case 4:  return QStringLiteral("Constructor");
    case 5:  return QStringLiteral("Field");
    case 6:  return QStringLiteral("Variable");
    case 7:  return QStringLiteral("Class");
    case 8:  return QStringLiteral("Interface");
    case 9:  return QStringLiteral("Module");
    case 10: return QStringLiteral("Property");
    case 11: return QStringLiteral("Unit");
    case 12: return QStringLiteral("Value");
    case 13: return QStringLiteral("Enum");
    case 14: return QStringLiteral("Keyword");
    case 15: return QStringLiteral("Snippet");
    case 18: return QStringLiteral("Reference");
    case 20: return QStringLiteral("EnumMember");
    case 21: return QStringLiteral("Constant");
    case 22: return QStringLiteral("Struct");
    case 23: return QStringLiteral("Event");
    case 24: return QStringLiteral("Operator");
    case 25: return QStringLiteral("TypeParameter");
    default: return QStringLiteral("Text");
    }
}

} // namespace StringUtils

#endif // STRINGUTILS_H
