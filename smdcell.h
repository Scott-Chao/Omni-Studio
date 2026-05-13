#ifndef SMDCELL_H
#define SMDCELL_H

#include <QFrame>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class CodeEditor;

class SmdCell : public QFrame
{
    Q_OBJECT

public:
    enum CellType { Markdown, Cpp, Python };

    explicit SmdCell(CellType type = Markdown, const QString &content = QString(),
                     QWidget *parent = nullptr);

    CellType cellType() const { return m_type; }
    void setCellType(CellType type);
    QString content() const;
    void setContent(const QString &text);
    bool isModified() const;

    void setCommandMode(bool cmd);
    bool isCommandMode() const { return m_commandMode; }
    void setActive(bool active);

    // MD rendering
    bool isRendered() const { return m_rendered; }
    void setRendered(bool rendered);

    // Output
    void showOutput(const QString &text, bool isStderr = false);
    void appendOutput(const QString &text, bool isStderr = false);
    void clearOutput();
    void hideOutput();

    QWidget *editorWidget() const;
    void setEditorFocus();

    void applyZoom(qreal factor, int baseFontSize);

    static CellType typeFromLangId(const QString &langId);
    static QString langIdFromType(CellType type);

signals:
    void executeRequested();
    void cellTypeChanged();
    void focusEntered();
    void focusLeft();
    void contentChanged();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi(CellType type);
    void setupMarkdownEditor();
    void setupCodeEditor(const QString &langId);
    void updateTypeLabel();
    void updateBorderStyle();
    void updateEditorHeight();

    CellType m_type;
    bool m_commandMode = false;
    bool m_active = false;
    bool m_rendered = false;

    QWidget *m_headerBar = nullptr;
    QLabel *m_typeLabel = nullptr;
    QLabel *m_executeHint = nullptr;
    QStackedWidget *m_editorStack = nullptr;   // page 0 = editor, page 1 = rendered view
    QPlainTextEdit *m_markdownEditor = nullptr;
    CodeEditor *m_codeEditor = nullptr;
    QTextBrowser *m_renderView = nullptr;
    QPlainTextEdit *m_outputArea = nullptr;

    QString m_languageId;
};

#endif // SMDCELL_H
