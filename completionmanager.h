#ifndef COMPLETIONMANAGER_H
#define COMPLETIONMANAGER_H

#include <QObject>
#include <QString>

#include "completionprovider.h"

class CompletionManager : public QObject
{
    Q_OBJECT

public:
    explicit CompletionManager(QObject *parent = nullptr);
    ~CompletionManager() override;

    void setLanguage(const QString &langId);
    QString languageId() const { return m_languageId; }

    bool isReady() const { return m_provider != nullptr; }

    void requestCompletion(const QString &text, int cursorPos);
    void requestHover(const QString &text, int cursorPos);
    void requestSignatureHelp(const QString &text, int cursorPos);

signals:
    void completionReady(QList<CompletionItem> items);
    void hoverReady(HoverInfo info);
    void signatureHelpReady(QList<SignatureInfo> signatures, int activeIndex);

private:
    void createProvider();

    CompletionProvider *m_provider = nullptr;
    QString m_languageId;
};

#endif // COMPLETIONMANAGER_H
