#ifndef UTILITIES_H
#define UTILITIES_H

#include <QString>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include <QDateTime>
#include <QStandardPaths>
#include <QIcon>
#include <QPainter>
#include <QColor>
#include <QRegularExpression>
#include <QScreen>
#include <QGuiApplication>
#include <QWidget>
#include "config/configmanager.h"

// ── String utilities ────────────────────────────────────────────────────────

namespace StringUtils {

// Normalize line-endings and lone surrogates for Python consumption.
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
    out = QString::fromUtf8(out.toUtf8());
    return out;
}

// Convert LSP CompletionItemKind integer code to human-readable string.
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

// ── Icon utilities ─────────────────────────────────────────────────────────

namespace IconUtils {

inline QIcon coloredSvgIcon(const QString &svgPath, const QColor &color, int size)
{
    QIcon src(svgPath);
    QPixmap srcPm = src.pixmap(size, size);
    if (srcPm.isNull())
        return src;
    QImage img = srcPm.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(img.rect(), color);
    p.end();
    return QIcon(QPixmap::fromImage(img));
}

} // namespace IconUtils

// ── File utilities ──────────────────────────────────────────────────────────

namespace TextFileUtils {

inline QStringList textExtensions()
{
    return ConfigManager::instance().textFileExtensions();
}

inline QStringList scanNameFilters()
{
    static const QStringList filters = []() {
        QStringList list;
        for (const QString &ext : textExtensions())
            list << "*." + ext;
        return list;
    }();
    return filters;
}

inline bool isTextExtension(const QString &suffix)
{
    return textExtensions().contains(suffix.toLower());
}

inline QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&file);
    return in.readAll();
}

inline bool writeTextFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(&file);
    out << content;
    return true;
}

inline bool isSafeRootPath(const QString &rootPath)
{
    return !rootPath.isEmpty()
        && !QDir(rootPath).isRoot()
        && rootPath != QDir::homePath();
}

inline QJsonDocument readJsonFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll());
}

inline bool writeJsonFile(const QString &path, const QJsonDocument &doc)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

} // namespace TextFileUtils

// ── Process utilities ───────────────────────────────────────────────────────

namespace ProcessUtils {

// Safely stop and schedule deletion of a QProcess.
inline void cleanup(QProcess *&process)
{
    if (!process)
        return;
    process->disconnect();
    if (process->state() != QProcess::NotRunning)
        process->kill();
    process->deleteLater();
    process = nullptr;
}

} // namespace ProcessUtils

// ── Markdown utilities ───────────────────────────────────────────────────────

namespace MarkdownUtils {

// Process inline formatting: `code`, **bold**, *italic*, [links](url)
inline QString processInline(const QString &text, const QColor &codeBg,
                              const QColor &codeFg, const QColor &linkColor)
{
    if (text.isEmpty())
        return {};

    QString result;
    result.reserve(text.size() + (text.size() / 4));

    int i = 0;
    const int len = text.length();

    while (i < len) {
        const QChar c = text[i];

        // Inline code `code` — processed first so its content is never
        // mistaken for bold/italic markers.
        if (c == QLatin1Char('`')) {
            int end = text.indexOf(QLatin1Char('`'), i + 1);
            if (end != -1) {
                result += QStringLiteral("<code style=\"background-color:%1; color:%2; "
                                        "padding:1px 4px; border-radius:3px; font-size:12px;\">")
                         .arg(codeBg.name(), codeFg.name())
                         + text.mid(i + 1, end - i - 1).toHtmlEscaped()
                         + QStringLiteral("</code>");
                i = end + 1;
                continue;
            }
        }

        // Link [label](url)
        if (c == QLatin1Char('[')) {
            int bracketEnd = text.indexOf(QStringLiteral("]("), i + 1);
            if (bracketEnd != -1) {
                int parenEnd = text.indexOf(QLatin1Char(')'), bracketEnd + 2);
                if (parenEnd != -1) {
                    result += QStringLiteral("<a href=\"")
                            + text.mid(bracketEnd + 2, parenEnd - bracketEnd - 2).toHtmlEscaped()
                            + QStringLiteral("\" style=\"color:") + linkColor.name()
                            + QStringLiteral(";\">")
                            + text.mid(i + 1, bracketEnd - i - 1).toHtmlEscaped()
                            + QStringLiteral("</a>");
                    i = parenEnd + 1;
                    continue;
                }
            }
        }

        // Bold **text** — checked before single-* italic
        if (c == QLatin1Char('*') && i + 1 < len && text[i + 1] == QLatin1Char('*')) {
            int end = text.indexOf(QStringLiteral("**"), i + 2);
            if (end != -1) {
                result += QStringLiteral("<b>")
                        + processInline(text.mid(i + 2, end - i - 2), codeBg, codeFg, linkColor)
                        + QStringLiteral("</b>");
                i = end + 2;
                continue;
            }
        }

        // Italic *text* — closing * must not be part of **, search past ** pairs
        if (c == QLatin1Char('*')) {
            int end = i + 1;
            while (end < len) {
                end = text.indexOf(QLatin1Char('*'), end);
                if (end == -1) break;
                if (end > i + 1 && (end + 1 >= len || text[end + 1] != QLatin1Char('*')))
                    break;  // valid closing *
                end += 2;  // skip past the whole ** pair and keep looking
            }
            if (end > i + 1) {
                result += QStringLiteral("<i>")
                        + processInline(text.mid(i + 1, end - i - 1), codeBg, codeFg, linkColor)
                        + QStringLiteral("</i>");
                i = end + 1;
                continue;
            }
        }

        // Regular character: HTML escape and append
        switch (c.unicode()) {
        case '<':  result += QStringLiteral("&lt;"); break;
        case '>':  result += QStringLiteral("&gt;"); break;
        case '&':  result += QStringLiteral("&amp;"); break;
        case '"':  result += QStringLiteral("&quot;"); break;
        default:   result += c; break;
        }
        ++i;
    }

    return result;
}

inline QString markdownToHtml(const QString &md, const QColor &textColor = QColor("#D4D4D4"),
                               const QColor &codeBg = QColor("#3C3C3C"),
                               const QColor &codeFg = QColor("#CE9178"),
                               const QColor &linkColor = QColor("#4DA3FF"),
                               const QColor &selectionBg = QColor("#264F78"),
                               const QColor &headingColor = QColor("#FFFFFF"))
{
    Q_UNUSED(selectionBg)
    if (md.isEmpty())
        return QStringLiteral("<p></p>");

    QString html;
    QStringList lines = md.split(QStringLiteral("\n"));

    bool inCodeBlock = false;
    QString codeBlockContent;
    bool inUL = false;
    bool inOL = false;

    for (const QString &rawLine : lines) {
        QString line = rawLine;

        if (line.startsWith(QStringLiteral("```"))) {
            if (inCodeBlock) {
                html += QStringLiteral("<pre style=\"background-color:%1; color:%2; "
                                       "padding:8px; border-radius:4px; font-size:12px; "
                                       "overflow-x:auto;\"><code>")
                      .arg(codeBg.name(), codeFg.name())
                      + codeBlockContent.toHtmlEscaped()
                      + QStringLiteral("</code></pre>\n");
                codeBlockContent.clear();
                inCodeBlock = false;
            } else {
                inCodeBlock = true;
                codeBlockContent.clear();
            }
            continue;
        }

        if (inCodeBlock) {
            if (!codeBlockContent.isEmpty())
                codeBlockContent += QStringLiteral("\n");
            codeBlockContent += line;
            continue;
        }

        if (line.trimmed().isEmpty()) {
            if (inUL) { html += QStringLiteral("</ul>\n"); inUL = false; }
            if (inOL) { html += QStringLiteral("</ol>\n"); inOL = false; }
            continue;
        }

        // Headings ## text
        static const QRegularExpression hRe(QStringLiteral("^(#{1,6})\\s+(.+)$"));
        QRegularExpressionMatch hMatch = hRe.match(line);
        if (hMatch.hasMatch()) {
            if (inUL) { html += QStringLiteral("</ul>\n"); inUL = false; }
            if (inOL) { html += QStringLiteral("</ol>\n"); inOL = false; }
            int level = hMatch.captured(1).length();
            QString headingText = processInline(hMatch.captured(2), codeBg, codeFg, linkColor);
            html += QStringLiteral("<h%1 style=\"color:%2; margin:8px 0 4px 0;\">%3</h%1>\n")
                        .arg(level).arg(headingColor.name(), headingText);
            continue;
        }

        // Unordered list - * item or - item
        static const QRegularExpression ulRe(QStringLiteral("^[\\*\\-]\\s+(.+)$"));
        QRegularExpressionMatch ulMatch = ulRe.match(line);
        if (ulMatch.hasMatch()) {
            if (inOL) { html += QStringLiteral("</ol>\n"); inOL = false; }
            if (!inUL) {
                html += QStringLiteral("<ul style=\"margin:4px 0; padding-left:20px;\">\n");
                inUL = true;
            }
            html += QStringLiteral("<li style=\"color:%1; margin:2px 0;\">%2</li>\n")
                        .arg(textColor.name(), processInline(ulMatch.captured(1), codeBg, codeFg, linkColor));
            continue;
        }

        // Numbered list 1. item
        static const QRegularExpression olRe(QStringLiteral("^\\d+\\.\\s+(.+)$"));
        QRegularExpressionMatch olMatch = olRe.match(line);
        if (olMatch.hasMatch()) {
            if (inUL) { html += QStringLiteral("</ul>\n"); inUL = false; }
            if (!inOL) {
                html += QStringLiteral("<ol style=\"margin:4px 0; padding-left:20px;\">\n");
                inOL = true;
            }
            html += QStringLiteral("<li style=\"color:%1; margin:2px 0;\">%2</li>\n")
                        .arg(textColor.name(), processInline(olMatch.captured(1), codeBg, codeFg, linkColor));
            continue;
        }

        // Horizontal rule ---
        static const QRegularExpression hrRe(QStringLiteral("^\\-{3,}\\s*$"));
        if (hrRe.match(line).hasMatch()) {
            if (inUL) { html += QStringLiteral("</ul>\n"); inUL = false; }
            if (inOL) { html += QStringLiteral("</ol>\n"); inOL = false; }
            html += QStringLiteral("<hr style=\"border:0; border-top:1px solid %1; margin:8px 0;\">\n")
                        .arg(codeBg.name());
            continue;
        }

        // Regular paragraph line
        if (inUL) { html += QStringLiteral("</ul>\n"); inUL = false; }
        if (inOL) { html += QStringLiteral("</ol>\n"); inOL = false; }
        html += QStringLiteral("<p style=\"color:%1; margin:4px 0;\">%2</p>\n")
                    .arg(textColor.name(), processInline(line, codeBg, codeFg, linkColor));
    }

    // Close any open lists at end of content
    if (inUL) html += QStringLiteral("</ul>\n");
    if (inOL) html += QStringLiteral("</ol>\n");

    // Close unclosed code block
    if (inCodeBlock) {
        html += QStringLiteral("<pre style=\"background-color:%1; color:%2; "
                               "padding:8px; border-radius:4px; font-size:12px; "
                               "overflow-x:auto;\"><code>")
              .arg(codeBg.name(), codeFg.name())
              + codeBlockContent.toHtmlEscaped()
              + QStringLiteral("</code></pre>\n");
    }

    return html;
}

} // namespace MarkdownUtils

// ── Screen utilities ─────────────────────────────────────────────────────────

namespace ScreenUtils {

// Clamp widget geometry to the screen's available geometry.
// Extracted from CompletionPopup and SignatureHelpPopup which had
// identical implementations.
inline void clampToScreen(QWidget *widget)
{
    QScreen *screen = widget->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    QRect sg = screen->availableGeometry();
    QRect geo = widget->geometry();
    if (geo.right() > sg.right()) widget->move(sg.right() - geo.width(), geo.y());
    if (geo.left() < sg.left())   widget->move(sg.left(), geo.y());
    if (geo.bottom() > sg.bottom()) widget->move(geo.x(), sg.bottom() - geo.height());
    if (geo.top() < sg.top())     widget->move(geo.x(), sg.top());
}

} // namespace ScreenUtils

// ── Debug logging ───────────────────────────────────────────────────────────

inline void debugLog(const QString &msg, const QString &filePath = QString())
{
    QString path = filePath;
    if (path.isEmpty()) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + QStringLiteral("/smd-debug");
        QDir().mkpath(dir);
        path = dir + QStringLiteral("/log.txt");
    }
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        s << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))
          << " [" << msg << "]\n";
    }
}

inline void clearLog(const QString &filePath = QString())
{
    QString path = filePath;
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
               + QStringLiteral("/smd-debug/log.txt");
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    }
}

#endif // UTILITIES_H
