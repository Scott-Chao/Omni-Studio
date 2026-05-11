#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QTextEdit>

class QSyntaxHighlighter;
class LineNumberArea;

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void setLanguage(const QString &langId);
    QString languageId() const { return m_languageId; }

    void setSearchHighlights(const QString &searchText);
    void clearSearchHighlights();
    void refreshLineNumberArea();

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

private:
    QSyntaxHighlighter *m_highlighter = nullptr;
    LineNumberArea *m_lineNumberArea;
    QString m_languageId;
    int m_indentWidth = 4;
    QList<QTextEdit::ExtraSelection> m_searchHighlights;

    void handleAutoIndent();
    bool handleBracketCompletion(QKeyEvent *event);
    bool handleBackspaceIndent(QKeyEvent *event);
    bool handleBackspacePairRemoval(QKeyEvent *event);
    bool handleTabKey(QKeyEvent *event);
    bool handleClosingBracketSkip(QKeyEvent *event);
    void handleToggleComment();
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
