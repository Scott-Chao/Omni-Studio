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

} // namespace StringUtils

#endif // STRINGUTILS_H
