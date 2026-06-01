#ifndef LSPUTILS_H
#define LSPUTILS_H

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include "completionprovider.h"
#include "core/utilities.h"

// ── Shared LSP response parsing utilities ───────────────────────────────
// Extracted from CppCompletionProvider and SmdLspManager which had
// identical implementations duplicated across both files.

namespace LspUtils {

// Convert absolute cursor position (0-based char offset) to LSP
// line (0-based) and character (0-based) coordinates.
// 6+ instances of this loop existed across cppcompletionprovider.cpp
// and smdlspmanager.cpp (CellCompletionAdapter).
inline void cursorToLineCol(const QString &text, int cursorPos,
                             int &line, int &col)
{
    line = 0;
    col = 0;
    int len = qMin(cursorPos, text.length());
    for (int i = 0; i < len; ++i) {
        if (text.at(i) == QLatin1Char('\n')) {
            ++line;
            col = 0;
        } else {
            ++col;
        }
    }
}

// Build standard initialize params (processId=null, rootUri=null,
// semanticTokens dynamicRegistration=true).
inline QJsonObject buildInitializeParams()
{
    QJsonObject params;
    params[QStringLiteral("processId")] = QJsonValue::Null;
    params[QStringLiteral("rootUri")] = QJsonValue::Null;

    QJsonObject textDocument;
    QJsonObject semanticTokens;
    semanticTokens[QStringLiteral("dynamicRegistration")] = true;
    textDocument[QStringLiteral("semanticTokens")] = semanticTokens;

    QJsonObject capabilities;
    capabilities[QStringLiteral("textDocument")] = textDocument;
    params[QStringLiteral("capabilities")] = capabilities;
    return params;
}

// Parse delta-encoded semantic tokens data[] array.
inline QList<SemanticToken> parseSemanticTokens(
    const QJsonObject &result,
    const QStringList &tokenTypeLegend,
    const QStringList &tokenModifierLegend)
{
    QList<SemanticToken> tokens;
    QJsonArray data = result.value(QStringLiteral("data")).toArray();
    if (data.isEmpty())
        return tokens;

    const int typeCount = tokenTypeLegend.size();
    const int modifierCount = tokenModifierLegend.size();

    tokens.reserve(data.size() / 5);

    int line = 0;
    int startChar = 0;

    for (int i = 0; i + 4 < data.size(); i += 5) {
        int deltaLine = data[i].toInt();
        int deltaStart = data[i + 1].toInt();
        int length = data[i + 2].toInt();
        int tokenType = data[i + 3].toInt();
        int tokenModifiers = data[i + 4].toInt();

        if (deltaLine > 0) {
            line += deltaLine;
            startChar = deltaStart;
        } else {
            startChar += deltaStart;
        }

        SemanticToken token;
        token.line = line;
        token.startChar = startChar;
        token.length = length;
        token.type = (tokenType >= 0 && tokenType < typeCount)
            ? tokenTypeLegend.at(tokenType) : QString();

        // Decode modifiers bitmask
        if (tokenModifiers > 0 && modifierCount > 0) {
            for (int bit = 0; bit < modifierCount; ++bit) {
                if (tokenModifiers & (1 << bit))
                    token.modifiers.append(tokenModifierLegend.at(bit));
            }
        }

        tokens.append(token);
    }

    return tokens;
}

// Parse a single CompletionItem from LSP JSON object.
// Replaces the identical textEdit/insertText/label priority logic.
inline CompletionItem parseCompletionItem(const QJsonObject &item)
{
    CompletionItem ci;

    // Prefer textEdit.newText > insertText > label
    QString insertText;
    QJsonValue textEditVal = item.value(QStringLiteral("textEdit"));
    if (textEditVal.isObject()) {
        insertText = textEditVal.toObject().value(QStringLiteral("newText")).toString();
    }
    if (insertText.isEmpty())
        insertText = item.value(QStringLiteral("insertText")).toString();
    if (insertText.isEmpty())
        insertText = item.value(QStringLiteral("label")).toString();

    ci.name = insertText.trimmed();
    ci.detail = item.value(QStringLiteral("detail")).toString();

    // Map LSP CompletionItemKind to type string
    int kind = item.value(QStringLiteral("kind")).toInt(0);
    ci.type = StringUtils::completionKindToString(kind);

    ci.signature = ci.name;

    // Extract documentation
    QJsonValue docVal = item.value(QStringLiteral("documentation"));
    if (docVal.isString()) {
        ci.doc = docVal.toString();
    } else if (docVal.isObject()) {
        ci.doc = docVal.toObject().value(QStringLiteral("value")).toString();
    }

    return ci;
}

// Parse Hover contents (allows object/string/array structures).
inline HoverInfo parseHoverContents(const QJsonValue &contentsVal)
{
    HoverInfo info;
    if (contentsVal.isUndefined() || contentsVal.isNull())
        return info;

    if (contentsVal.isArray()) {
        QJsonArray arr = contentsVal.toArray();
        QStringList docParts;
        for (const QJsonValue &v : arr) {
            if (v.isObject()) {
                QJsonObject obj = v.toObject();
                if (obj.contains(QStringLiteral("language")))
                    info.signature = obj.value(QStringLiteral("value")).toString();
                else
                    docParts.append(obj.value(QStringLiteral("value")).toString());
            } else if (v.isString()) {
                docParts.append(v.toString());
            }
        }
        info.doc = docParts.join(QStringLiteral("\n"));
        return info;
    }

    if (contentsVal.isString()) {
        info.doc = contentsVal.toString();
        return info;
    }

    QJsonObject contents = contentsVal.toObject();
    if (contents.contains(QStringLiteral("language"))) {
        info.signature = contents.value(QStringLiteral("value")).toString();
    } else {
        info.doc = contents.value(QStringLiteral("value")).toString();
    }

    return info;
}

// Parse a single SignatureInformation from LSP JSON object.
// Handles label as string or [start,end] offset pairs.
inline SignatureInfo parseSignatureInfo(const QJsonObject &sig)
{
    SignatureInfo info;
    info.label = sig.value(QStringLiteral("label")).toString();
    info.doc = sig.value(QStringLiteral("documentation")).toString();

    QJsonValue paramsVal = sig.value(QStringLiteral("parameters"));
    if (paramsVal.isArray()) {
        QJsonArray params = paramsVal.toArray();
        for (const QJsonValue &pv : params) {
            QJsonObject pObj = pv.toObject();
            QJsonValue labelVal = pObj.value(QStringLiteral("label"));
            if (labelVal.isString()) {
                info.parameters.append(labelVal.toString());
            } else if (labelVal.isArray()) {
                QJsonArray offsets = labelVal.toArray();
                if (offsets.size() >= 2) {
                    int start = offsets.at(0).toInt();
                    int end = offsets.at(1).toInt();
                    if (start >= 0 && end <= info.label.length() && start < end)
                        info.parameters.append(info.label.mid(start, end - start));
                }
            }
        }
    }

    return info;
}

} // namespace LspUtils

#endif // LSPUTILS_H
