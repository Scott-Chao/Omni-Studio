#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QTextEdit>
#include <QList>

#include "completionprovider.h"
#include "smddiagnostic.h"

class QSyntaxHighlighter;
class CompletionProvider;
class CompletionPopup;
class HoverManager;
class SignatureHelpManager;
class LineNumberArea;

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void setLanguage(const QString &langId);
    void setLanguageSyntaxOnly(const QString &langId);
    QString languageId() const { return m_languageId; }
    void setDocumentUri(const QString &uri);

    CompletionProvider *completionProvider() const { return m_completionProvider; }

    void setCompletionProvider(CompletionProvider *provider);

    void setDiagnostics(const QList<SmdDiagnostic> &diagnostics);
    QList<SmdDiagnostic> diagnostics() const { return m_diagnostics; }
    void clearDiagnostics();
    const SmdDiagnostic* diagnosticAt(int line, int col) const;

    void setIndentWidth(int width);
    int indentWidth() const { return m_indentWidth; }

    void reloadColors();
    void reloadShortcuts();

    void hideSignatureHelp();
    void setSearchHighlights(const QString &searchText);
    void clearSearchHighlights();
    void clearCurrentLineHighlight();
    void refreshCurrentLineHighlight();
    void refreshLineNumberArea();

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;
    bool isPositionOverText(const QPoint &viewportPos) const;

signals:
    void diagnosticsToggleRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();
    void onServerReady();
    void onEditorTextChanged();
    void onCompletionsReady(QList<CompletionItem> items);
    void onProviderFailed(const QString &reason);

private:
    void createCompletionProvider(const QString &langId);

    QSyntaxHighlighter *m_highlighter = nullptr;
    void updateExtraSelectionsWithDiagnostics();

    CompletionProvider *m_completionProvider = nullptr;
    bool m_ownsProvider = true;
    CompletionPopup *m_completionPopup = nullptr;
    HoverManager *m_hoverManager = nullptr;
    SignatureHelpManager *m_signatureHelpManager = nullptr;
    LineNumberArea *m_lineNumberArea;
    QString m_languageId;
    QString m_documentUri;
    int m_indentWidth = 4;
    QColor m_cachedLnBg;
    QColor m_cachedLnFg;
    QColor m_cachedCurrentLine;
    QList<QTextEdit::ExtraSelection> m_searchHighlights;
    QString m_searchHighlightText;
    QList<SmdDiagnostic> m_diagnostics;

    // Configurable shortcuts
    QKeySequence m_completionTrigger;
    QKeySequence m_indentRight;
    QKeySequence m_indentLeft;
    QKeySequence m_toggleComment;
    QKeySequence m_toggleDiagnostics;

    void handleAutoIndent();
    bool handleBracketCompletion(QKeyEvent *event);
    bool handleBackspaceIndent(QKeyEvent *event);
    bool handleBackspacePairRemoval(QKeyEvent *event);
    bool handleTabKey(QKeyEvent *event);
    bool handleClosingBracketSkip(QKeyEvent *event);
    void handleToggleComment();
    void handleIndentLeft();
    void handleIndentRight();
    void triggerCompletion();
    void insertCompletion(const CompletionItem &item);
    QString commentPrefix() const;
    bool isCursorInStringOrComment() const;
    QString indentString() const;
};

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditor *m_codeEditor;
};

#endif // CODEEDITOR_H
