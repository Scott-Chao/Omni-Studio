#ifndef COMPLETIONPROVIDER_H
#define COMPLETIONPROVIDER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QIcon>

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

signals:
    void completionReady(QList<CompletionItem> items);
    void hoverReady(HoverInfo info);
    void signatureHelpReady(QList<SignatureInfo> signatures, int activeIndex);
};

#endif // COMPLETIONPROVIDER_H
