#ifndef COMPLETIONPROVIDER_H
#define COMPLETIONPROVIDER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QTimer>

#include "smddiagnostic.h"

struct SemanticToken {
    int line = 0;       // 0-based line number
    int startChar = 0;  // 0-based character offset within the line
    int length = 0;
    QString type;       // "function", "method", "class", "parameter", "variable", etc.
    QStringList modifiers;
};

struct CompletionItem {
    QString name;           // 符号名
    QString type;           // "function", "class", "variable", "method", "keyword"
    QString signature;      // 完整签名文字（可选）
    QString detail;         // 额外信息（如所属模块）
    QString doc;            // 文档（可选）
    QIcon icon;             // 类型图标
};

struct HoverInfo {
    QString signature;      // 完整类型签名
    QString doc;            // 文档说明
    QString definition;     // 定义位置
};

struct SignatureInfo {
    QString label;          // 完整签名文字
    QString doc;            // 文档
    QStringList parameters; // 参数列表
    int activeParameter = 0; // 当前参数索引
};

class CompletionProvider : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual ~CompletionProvider() = default;

    virtual void requestCompletion(const QString &text, int cursorPos) = 0;
    virtual void requestHover(const QString &text, int cursorPos) = 0;
    virtual void requestSignatureHelp(const QString &text, int cursorPos) = 0;

    // Document sync (default no-op for fallback providers)
    virtual void openDocument(const QString &uri, const QString &languageId, const QString &text)
    {
        Q_UNUSED(uri); Q_UNUSED(languageId); Q_UNUSED(text);
    }
    virtual void updateText(const QString &text)
    {
        Q_UNUSED(text);
    }

signals:
    void completionReady(QList<CompletionItem> items);
    void hoverReady(HoverInfo info);
    void signatureHelpReady(QList<SignatureInfo> signatures, int activeIndex);
    void diagnosticsUpdated(QList<SmdDiagnostic> diagnostics);
    void semanticTokensReady(QList<SemanticToken> tokens);
    void serverReady();
    void serverFailed(const QString &reason);

protected:
    enum class PendingRequest { None, Completion, Hover, SignatureHelp, SemanticTokens, Diagnostics };

    void emitEmptyResults()
    {
        PendingRequest pending = m_pendingRequest;
        m_pendingRequest = PendingRequest::None;
        m_requestTimer.stop();

        switch (pending) {
        case PendingRequest::Completion:
            emit completionReady({}); break;
        case PendingRequest::Hover:
            emit hoverReady({}); break;
        case PendingRequest::SignatureHelp:
            emit signatureHelpReady({}, 0); break;
        case PendingRequest::SemanticTokens:
            emit semanticTokensReady({}); break;
        case PendingRequest::Diagnostics:
            emit diagnosticsUpdated({}); break;
        default:
            break;
        }
    }

    PendingRequest m_pendingRequest = PendingRequest::None;
    QTimer m_requestTimer;
};

#endif // COMPLETIONPROVIDER_H
