#include "smdcell.h"
#include "codeeditor.h"
#include "languageutils.h"
#include "configmanager.h"
#include "settingsmanager.h"
#include <QTimer>
#include <QTextBlock>
#include <QTextLayout>

SmdCell::SmdCell(CellType type, const QString &content, QWidget *parent)
    : QFrame(parent)
    , m_type(type)
    , m_languageId(langIdFromType(type))
{
    setupUi(type);
    if (!content.isEmpty())
        setContent(content);
}

void SmdCell::setupUi(CellType type)
{
    setFrameStyle(QFrame::NoFrame);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header bar
    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(24);
    auto *headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(8, 2, 8, 2);
    headerLayout->setSpacing(6);

    m_typeLabel = new QLabel(m_headerBar);
    m_typeLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e0e0e0; font-size: 10px; padding: 1px 6px; "
        "border-radius: 3px; background: %1; }"
    ).arg(m_type == Markdown ? QStringLiteral("#3a6ea5")
          : m_type == Cpp ? QStringLiteral("#2d8a56") : QStringLiteral("#b8952e")));
    headerLayout->addWidget(m_typeLabel);

    headerLayout->addStretch();

    m_executeHint = new QLabel(m_headerBar);
    m_executeHint->setStyleSheet(QStringLiteral("color: #858585; font-size: 10px;"));
    m_executeHint->setVisible(false);
    headerLayout->addWidget(m_executeHint);

    mainLayout->addWidget(m_headerBar);

    // Editor/View stack — no stretch, height driven by content
    m_editorStack = new QStackedWidget(this);
    m_editorStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    if (type == Markdown)
        setupMarkdownEditor();
    else
        setupCodeEditor(m_languageId);

    // Render view (page 1)
    m_renderView = new QTextBrowser(this);
    m_renderView->setOpenExternalLinks(false);
    m_renderView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_renderView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_renderView->setStyleSheet(QStringLiteral(
        "QTextBrowser { background-color: #1E1E1E; color: #D4D4D4; border: none; "
        "selection-background-color: #264F78; }"
    ));
    m_renderView->document()->setDefaultStyleSheet(QStringLiteral(
        "body { color: #D4D4D4; background-color: #1E1E1E; }"
        "h1 { color: #569CD6; } h2 { color: #569CD6; } h3 { color: #569CD6; }"
        "code { background-color: #2D2D30; padding: 2px 4px; border-radius: 3px; }"
        "pre { background-color: #2D2D30; padding: 8px; border-radius: 4px; }"
        "a { color: #4EC9B0; }"
    ));
    m_renderView->installEventFilter(this);
    m_editorStack->addWidget(m_renderView);

    mainLayout->addWidget(m_editorStack);

    // Output area (hidden by default)
    m_outputArea = new QPlainTextEdit(this);
    m_outputArea->setReadOnly(true);
    m_outputArea->setMaximumBlockCount(
        ConfigManager::instance().outputPanelMaxBlocks());
    m_outputArea->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background-color: #1E1E1E; color: #D4D4D4; "
        "selection-background-color: #264F78; border: none; "
        "border-top: 1px solid #3c3c3c; }"
    ));
    QFont outFont(QStringLiteral("Consolas"), 10);
    outFont.setStyleHint(QFont::Monospace);
    m_outputArea->setFont(outFont);
    m_outputArea->setVisible(false);
    m_outputArea->setMinimumHeight(40);
    m_outputArea->setMaximumHeight(200);
    mainLayout->addWidget(m_outputArea);

    updateTypeLabel();
    updateBorderStyle();
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void SmdCell::setupMarkdownEditor()
{
    m_markdownEditor = new QPlainTextEdit(this);
    m_markdownEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_markdownEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_markdownEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_markdownEditor->document()->setDocumentMargin(0);
    const auto &cfg = ConfigManager::instance();
    auto &sm = SettingsManager::instance();
    QString family = sm.value("editor.font.family", cfg.editorFontFamily()).toString();
    int size = sm.value("editor.font.size", cfg.editorFontSize()).toInt();
    QFont f(family, size);
    f.setStyleHint(QFont::Monospace);
    m_markdownEditor->setFont(f);
    m_markdownEditor->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background-color: #1E1E1E; color: #D4D4D4; "
        "selection-background-color: #264F78; border: none; padding: 8px; }"
    ));
    m_markdownEditor->setTabChangesFocus(false);

    connect(m_markdownEditor->document(), &QTextDocument::blockCountChanged,
            this, &SmdCell::updateEditorHeight);
    connect(m_markdownEditor->document(), &QTextDocument::contentsChanged,
            this, &SmdCell::updateEditorHeight);
    m_markdownEditor->installEventFilter(this);
    m_editorStack->insertWidget(0, m_markdownEditor);
    QTimer::singleShot(0, this, &SmdCell::updateEditorHeight);
}

void SmdCell::setupCodeEditor(const QString &langId)
{
    m_codeEditor = new CodeEditor(this);
    m_codeEditor->setLanguage(langId);
    m_codeEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_codeEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_codeEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_codeEditor->setTabChangesFocus(false);
    m_codeEditor->document()->setDocumentMargin(0);

    connect(m_codeEditor->document(), &QTextDocument::blockCountChanged,
            this, &SmdCell::updateEditorHeight);
    connect(m_codeEditor->document(), &QTextDocument::contentsChanged,
            this, &SmdCell::updateEditorHeight);
    m_codeEditor->installEventFilter(this);
    m_editorStack->insertWidget(0, m_codeEditor);
    QTimer::singleShot(0, this, &SmdCell::updateEditorHeight);
}

void SmdCell::setCellType(CellType type)
{
    if (m_type == type)
        return;

    QString oldContent = content();
    m_type = type;
    m_languageId = langIdFromType(type);
    m_rendered = false;

    // Remove old editor from stack
    if (m_markdownEditor) {
        m_editorStack->removeWidget(m_markdownEditor);
        m_markdownEditor->deleteLater();
        m_markdownEditor = nullptr;
    }
    if (m_codeEditor) {
        m_editorStack->removeWidget(m_codeEditor);
        m_codeEditor->deleteLater();
        m_codeEditor = nullptr;
    }

    if (type == Markdown) {
        setupMarkdownEditor();
    } else {
        setupCodeEditor(m_languageId);
    }

    m_editorStack->setCurrentIndex(0);
    if (!oldContent.isEmpty())
        setContent(oldContent);
    updateEditorHeight();

    updateTypeLabel();
    updateBorderStyle();
    emit cellTypeChanged();
}

QString SmdCell::content() const
{
    if (m_type == Markdown)
        return m_markdownEditor ? m_markdownEditor->toPlainText() : QString();
    return m_codeEditor ? m_codeEditor->toPlainText() : QString();
}

void SmdCell::setContent(const QString &text)
{
    if (m_type == Markdown && m_markdownEditor)
        m_markdownEditor->setPlainText(text);
    else if (m_codeEditor)
        m_codeEditor->setPlainText(text);
    QTimer::singleShot(0, this, &SmdCell::updateEditorHeight);
}

bool SmdCell::isModified() const
{
    if (m_type == Markdown && m_markdownEditor)
        return m_markdownEditor->document()->isModified();
    if (m_codeEditor)
        return m_codeEditor->document()->isModified();
    return false;
}

void SmdCell::setCommandMode(bool cmd)
{
    m_commandMode = cmd;
    updateBorderStyle();
    m_executeHint->setVisible(cmd && m_rendered);
    if (cmd && m_rendered)
        m_executeHint->setText(QStringLiteral("Ctrl+Shift+Z: 编辑"));
}

void SmdCell::setActive(bool active)
{
    m_active = active;
    updateBorderStyle();
    if (m_codeEditor) {
        if (active)
            m_codeEditor->refreshCurrentLineHighlight();
        else
            m_codeEditor->clearCurrentLineHighlight();
    }
}

void SmdCell::setRendered(bool rendered)
{
    if (m_type != Markdown)
        return;
    m_rendered = rendered;
    if (rendered) {
        m_renderView->setMarkdown(content());
        m_editorStack->setCurrentIndex(1);
        QTimer::singleShot(0, this, &SmdCell::updateEditorHeight);
    } else {
        m_editorStack->setCurrentIndex(0);
        if (m_markdownEditor)
            m_markdownEditor->setFocus();
        updateEditorHeight();
    }
    if (m_commandMode)
        m_executeHint->setVisible(rendered);
}

void SmdCell::showOutput(const QString &text, bool isStderr)
{
    m_outputArea->setVisible(true);
    m_outputArea->clear();
    appendOutput(text, isStderr);
}

void SmdCell::appendOutput(const QString &text, bool isStderr)
{
    m_outputArea->setVisible(true);
    QTextCursor cursor = m_outputArea->textCursor();
    cursor.movePosition(QTextCursor::End);

    if (isStderr) {
        QTextCharFormat fmt;
        fmt.setForeground(QColor(QStringLiteral("#F48771")));
        cursor.insertText(text, fmt);
    } else {
        cursor.insertText(text);
    }
    m_outputArea->ensureCursorVisible();
}

void SmdCell::clearOutput()
{
    m_outputArea->clear();
}

void SmdCell::hideOutput()
{
    m_outputArea->setVisible(false);
    m_outputArea->clear();
}

QWidget *SmdCell::editorWidget() const
{
    if (m_rendered)
        return m_renderView;
    if (m_type == Markdown)
        return m_markdownEditor;
    return m_codeEditor;
}

void SmdCell::setEditorFocus()
{
    if (m_rendered) {
        setRendered(false);
        return;
    }
    QWidget *w = editorWidget();
    if (w) {
        w->setFocus();
        if (auto *pte = qobject_cast<QPlainTextEdit*>(w))
            pte->moveCursor(QTextCursor::End);
    }
}

void SmdCell::applyZoom(qreal factor, int baseFontSize)
{
    const auto &cfg = ConfigManager::instance();
    int pointSize = qBound(cfg.fontMinPointSize(),
                           qRound(baseFontSize * factor),
                           cfg.fontMaxPointSize());

    if (m_markdownEditor) {
        QFont f = m_markdownEditor->font();
        f.setPointSize(pointSize);
        m_markdownEditor->setFont(f);
    }
    if (m_codeEditor) {
        QFont f = m_codeEditor->font();
        f.setPointSize(pointSize);
        m_codeEditor->setFont(f);
        m_codeEditor->refreshLineNumberArea();
    }

    // repaint() only lays out blocks visible in the current viewport. After
    // setFont() the viewport is sized for the old font — blocks outside the
    // visible area won't be relaid out, causing layout->boundingRect() to
    // return 0 and forcing fallback to font metrics. Temporarily stretch the
    // editor so repaint() covers ALL blocks, then measure the correct height.
    QPlainTextEdit *ed = m_markdownEditor
        ? static_cast<QPlainTextEdit*>(m_markdownEditor)
        : static_cast<QPlainTextEdit*>(m_codeEditor);
    if (ed) {
        ed->setFixedHeight(50000);
        m_editorStack->setFixedHeight(50000);
        ed->repaint();
    }

    updateEditorHeight();
}

void SmdCell::updateEditorHeight()
{
    QPlainTextEdit *ed = nullptr;
    if (m_rendered) {
        int docH = static_cast<int>(m_renderView->document()->size().height());
        m_renderView->setFixedHeight(docH + 16);
        m_editorStack->setFixedHeight(docH + 16);
        updateGeometry();
        return;
    }
    if (m_type == Markdown)
        ed = m_markdownEditor;
    else if (m_codeEditor)
        ed = m_codeEditor;

    if (!ed)
        return;

    // Measure actual content height by summing each block's QTextLayout
    // bounding rect. QFontMetrics::lineSpacing() is integer-truncated and
    // differs from QPlainTextEdit's internal layout by ~0.1px/line, which
    // accumulates to visible scroll overflow on multi-line blocks.
    QFontMetricsF fmf(ed->font());
    qreal fallbackLH = fmf.lineSpacing();
    qreal totalDocH = 0;
    int visualLines = 0;

    for (QTextBlock block = ed->document()->begin(); block.isValid(); block = block.next()) {
        QTextLayout *layout = block.layout();
        int lc = 1;
        if (layout) {
            lc = layout->lineCount();
            if (lc < 1) lc = 1;
            qreal h = layout->boundingRect().height();
            if (h > 0)
                totalDocH += h;
            else
                totalDocH += fallbackLH * lc;
        } else {
            totalDocH += fallbackLH * lc;
        }
        visualLines += lc;
    }
    if (visualLines < 1)
        visualLines = 1;

    QMargins cm = ed->contentsMargins();
    int marginH = cm.top() + cm.bottom();
    int contentH = qCeil(totalDocH) + marginH + 2;
    int minH = qCeil(fallbackLH) + marginH + 2;
    if (contentH < minH)
        contentH = minH;

    ed->setFixedHeight(contentH);
    m_editorStack->setFixedHeight(contentH);
    updateGeometry();
    emit contentChanged();
}

void SmdCell::updateTypeLabel()
{
    QString text;
    QString color;
    switch (m_type) {
    case Markdown:
        text = QStringLiteral("MD");
        color = QStringLiteral("#3a6ea5");
        break;
    case Cpp:
        text = QStringLiteral("C++");
        color = QStringLiteral("#2d8a56");
        break;
    case Python:
        text = QStringLiteral("Python");
        color = QStringLiteral("#b8952e");
        break;
    }
    m_typeLabel->setText(text);
    m_typeLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e0e0e0; font-size: 10px; padding: 1px 6px; "
        "border-radius: 3px; background: %1; }"
    ).arg(color));
}

void SmdCell::updateBorderStyle()
{
    if (m_active) {
        setStyleSheet(QStringLiteral(
            "SmdCell { border: 2px solid #0078d4; "
            "background-color: #252526; }"
        ));
    } else if (m_commandMode) {
        setStyleSheet(QStringLiteral(
            "SmdCell { border: 2px solid #3c3c3c; "
            "background-color: #1E1E1E; }"
        ));
    } else {
        setStyleSheet(QStringLiteral(
            "SmdCell { border: 2px solid transparent; "
            "background-color: #1E1E1E; }"
        ));
    }
}

SmdCell::CellType SmdCell::typeFromLangId(const QString &langId)
{
    if (langId == QStringLiteral("cpp"))
        return Cpp;
    if (langId == QStringLiteral("python"))
        return Python;
    return Markdown;
}

bool SmdCell::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FocusIn) {
        emit focusEntered();
    }
    return QFrame::eventFilter(obj, event);
}

QString SmdCell::langIdFromType(CellType type)
{
    switch (type) {
    case Markdown: return QStringLiteral("markdown");
    case Cpp:      return QStringLiteral("cpp");
    case Python:   return QStringLiteral("python");
    }
    return QStringLiteral("markdown");
}
