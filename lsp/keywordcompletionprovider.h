#ifndef KEYWORDCOMPLETIONPROVIDER_H
#define KEYWORDCOMPLETIONPROVIDER_H

#include "completionprovider.h"

class KeywordCompletionProvider : public CompletionProvider
{
    Q_OBJECT

public:
    explicit KeywordCompletionProvider(const QString &languageId, QObject *parent = nullptr);

    void requestCompletion(const QString &text, int cursorPos) override;
    void requestHover(const QString &text, int cursorPos) override;
    void requestSignatureHelp(const QString &text, int cursorPos) override;

private:
    QString m_languageId;

    QStringList keywordsForLanguage() const;
    QStringList extractWords(const QString &text) const;
};

#endif // KEYWORDCOMPLETIONPROVIDER_H
