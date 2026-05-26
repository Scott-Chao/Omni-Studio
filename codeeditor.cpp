#include "codeeditor.h"
#include "cppcompletionprovider.h"
#include "pythoncompletionprovider.h"
#include "keywordcompletionprovider.h"
#include "cppsyntaxhighlighter.h"
#include "pythonsyntaxhighlighter.h"
#include "smddiagnostic.h"
#include "debuglog.h"
#include "completionpopup.h"
#include "hovermanager.h"
#include "signaturehelpmanager.h"
#include "languageutils.h"
#include "configmanager.h"
#include "thememanager.h"
#include "settingsmanager.h"
#include <QPainter>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextDocument>
#include <QKeyEvent>
#include <QSyntaxHighlighter>
#include <QMouseEvent>
#include <QApplication>
#include <QPointer>
#include <QAbstractNativeEventFilter>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// ============================================================
// EscNativeFilter — catches VK_ESCAPE at the Windows message level
// before Qt gets a chance to route it to the wrong HWND.
// ============================================================

class EscNativeFilter : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit EscNativeFilter(QObject *parent = nullptr) : QObject(parent) {}
    QPointer<CompletionPopup> popup;
    QPointer<SignatureHelpManager> sigMgr;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override
    {
        Q_UNUSED(result);
#ifdef Q_OS_WIN
        if (eventType == "windows_generic_MSG") {
            MSG *msg = static_cast<MSG *>(message);
            if (msg->message == WM_KEYDOWN && msg->wParam == VK_ESCAPE) {
                if (sigMgr && sigMgr->isActive()) {
                    sigMgr->hide();
                    return true;
                }
                if (popup && popup->isActive()) {
                    popup->hide();
                    return true;
                }
            }
        }
#else
        Q_UNUSED(eventType);
        Q_UNUSED(message);
#endif
        return false;
    }
};

// ============================================================
// LineNumberArea
// ============================================================

LineNumberArea::LineNumberArea(CodeEditor *editor)
    : QWidget(editor)
    , m_codeEditor(editor)
{
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    m_codeEditor->lineNumberAreaPaintEvent(event);
}

// ============================================================
// CodeEditor
// ============================================================

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    const auto &tm = ThemeManager::instance();
    const auto &cfg = ConfigManager::instance();
    auto &sm = SettingsManager::instance();

    // Editor theme colors (updated on theme change)
    setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; color: %2; "
        "selection-background-color: %3; }"
    )
        .arg(tm.color("editor.background").name())
        .arg(tm.color("editor.foreground").name())
        .arg(tm.color("editor.selectionBackground").name()));

    QString fontFamily = sm.value("editor.font.family", cfg.editorFontFamily()).toString();
    int fontSize = sm.value("editor.font.size", cfg.editorFontSize()).toInt();
    QFont font(fontFamily, fontSize);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * m_indentWidth);

    // Cache paint-time colors from ThemeManager
    m_cachedLnBg = tm.color("editorLineNumber.background");
    m_cachedLnFg = tm.color("editorLineNumber.foreground");
    m_cachedCurrentLine = tm.color("editor.lineHighlightBackground");

    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_lineNumberArea = new LineNumberArea(this);
    m_completionPopup = new CompletionPopup(this);
    // m_completionProvider is created in setLanguage()
    m_hoverManager = new HoverManager(this, nullptr, this);
    m_signatureHelpManager = new SignatureHelpManager(this, nullptr, this);

    // Native Windows event filter — the only way to catch Esc when
    // a Qt::Tool window is visible (Windows routes Esc to the Tool HWND).
    {
        auto *nativeFilter = new EscNativeFilter(this);
        nativeFilter->popup = m_completionPopup;
        nativeFilter->sigMgr = m_signatureHelpManager;
        qApp->installNativeEventFilter(nativeFilter);
    }

    viewport()->installEventFilter(this);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    // Load configurable shortcuts
    reloadShortcuts();

    // Also intercept ShortcutOverride on this widget (not just viewport)
    installEventFilter(this);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &CodeEditor::reloadColors);
}

void CodeEditor::reloadShortcuts()
{
    auto &sm = SettingsManager::instance();
    m_completionTrigger = QKeySequence(sm.value("shortcuts.completion_trigger", "Ctrl+I").toString());
    m_indentRight = QKeySequence(sm.value("shortcuts.indent_right", "Ctrl+]").toString());
    m_indentLeft = QKeySequence(sm.value("shortcuts.indent_left", "Ctrl+[").toString());
    m_toggleComment = QKeySequence(sm.value("shortcuts.toggle_comment", "Ctrl+/").toString());
    m_toggleDiagnostics = QKeySequence(sm.value("shortcuts.toggle_diagnostics",
        ConfigManager::instance().shortcut("toggle_diagnostics", "Ctrl+D")).toString());
    debugLog(QString("CodeEditor::reloadShortcuts toggle_diagnostics=%1")
        .arg(m_toggleDiagnostics.toString()));
}

void CodeEditor::setIndentWidth(int width)
{
    m_indentWidth = width;
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * m_indentWidth);
}

void CodeEditor::reloadColors()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; color: %2; "
        "selection-background-color: %3; }"
    )
        .arg(tm.color("editor.background").name())
        .arg(tm.color("editor.foreground").name())
        .arg(tm.color("editor.selectionBackground").name()));

    m_cachedLnBg = tm.color("editorLineNumber.background");
    m_cachedLnFg = tm.color("editorLineNumber.foreground");
    m_cachedCurrentLine = tm.color("editor.lineHighlightBackground");

    // Rebuild search highlights with new theme colors
    if (!m_searchHighlightText.isEmpty()) {
        m_searchHighlights.clear();
        QTextDocument *doc = document();
        QTextCursor searchCursor(doc);
        while (true) {
            QTextCursor found = doc->find(m_searchHighlightText, searchCursor);
            if (found.isNull())
                break;
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(tm.color("search.highlightBackground"));
            sel.format.setForeground(tm.color("search.highlightForeground"));
            sel.cursor = found;
            m_searchHighlights.append(sel);
            searchCursor = found;
            searchCursor.movePosition(QTextCursor::EndOfWord);
        }
    }

    highlightCurrentLine();
    m_lineNumberArea->update();
}

void CodeEditor::setDocumentUri(const QString &uri)
{
    m_documentUri = uri;
}

void CodeEditor::setLanguage(const QString &langId)
{
    if (m_highlighter) {
        delete m_highlighter;
        m_highlighter = nullptr;
    }
    m_languageId = langId;
    m_highlighter = LanguageUtils::createHighlighter(langId, document());

    createCompletionProvider(langId);

    // Connect text sync after the provider is set up
    disconnect(document(), &QTextDocument::contentsChanged,
               this, &CodeEditor::onEditorTextChanged);
    connect(document(), &QTextDocument::contentsChanged,
            this, &CodeEditor::onEditorTextChanged);

    // Open document with current content (triggers didOpen when server is ready)
    if (m_completionProvider)
        m_completionProvider->openDocument(m_documentUri, langId, toPlainText());
}

void CodeEditor::setLanguageSyntaxOnly(const QString &langId)
{
    if (m_highlighter) {
        delete m_highlighter;
        m_highlighter = nullptr;
    }
    m_languageId = langId;
    m_highlighter = LanguageUtils::createHighlighter(langId, document());

    // Connect text sync for when a shared provider is set later
    disconnect(document(), &QTextDocument::contentsChanged,
               this, &CodeEditor::onEditorTextChanged);
    connect(document(), &QTextDocument::contentsChanged,
            this, &CodeEditor::onEditorTextChanged);
}

void CodeEditor::onServerReady()
{
    // Server just became ready (initial start or restart).
    // Open (or re-open) the document so clangd knows about its content.
    if (m_completionProvider)
        m_completionProvider->openDocument(m_documentUri, m_languageId, toPlainText());
}

void CodeEditor::onEditorTextChanged()
{
    if (m_completionProvider)
        m_completionProvider->updateText(toPlainText());
}

void CodeEditor::triggerCompletion()
{
    if (!m_completionProvider)
        return;

    QString text = toPlainText();
    if (text.length() > 1024 * 1024) // >1MB: skip for performance
        return;

    QTextCursor cursor = textCursor();
    m_completionProvider->requestCompletion(text, cursor.position());
}

// ---- Completion provider lifecycle ----

void CodeEditor::createCompletionProvider(const QString &langId)
{
    // Delete old provider — disconnect signals first to prevent stale callbacks
    if (m_completionProvider) {
        disconnect(m_completionProvider, nullptr, this, nullptr);
        disconnect(m_completionProvider, nullptr, m_hoverManager, nullptr);
        disconnect(m_completionProvider, nullptr, m_signatureHelpManager, nullptr);
        m_completionProvider->deleteLater();
        m_completionProvider = nullptr;
    }

    if (langId == QStringLiteral("cpp")) {
        auto *cpp = new CppCompletionProvider(this);
        m_completionProvider = cpp;

        connect(cpp, &CppCompletionProvider::serverReady,
                this, &CodeEditor::onServerReady);
        connect(cpp, &CppCompletionProvider::serverFailed,
                this, &CodeEditor::onProviderFailed);
    } else if (langId == QStringLiteral("python")) {
        auto *py = new PythonCompletionProvider(this);
        m_completionProvider = py;

        connect(py, &PythonCompletionProvider::serverReady,
                this, &CodeEditor::onServerReady);
        connect(py, &PythonCompletionProvider::serverFailed,
                this, &CodeEditor::onProviderFailed);
    } else {
        return;
    }

    // Common signal connections
    connect(m_completionProvider, &CompletionProvider::completionReady,
            this, &CodeEditor::onCompletionsReady);
    connect(m_completionProvider, &CompletionProvider::diagnosticsUpdated,
            this, &CodeEditor::setDiagnostics);
    connect(m_completionProvider, &CompletionProvider::semanticTokensReady,
            this, [this](const QList<SemanticToken> &tokens) {
        if (m_highlighter) {
            if (auto *cppHL = qobject_cast<CppSyntaxHighlighter*>(m_highlighter))
                cppHL->setSemanticTokens(tokens);
            else if (auto *pyHL = qobject_cast<PythonSyntaxHighlighter*>(m_highlighter))
                pyHL->setSemanticTokens(tokens);
        }
        emit semanticTokensApplied();
    });

    // Notify hover and signature managers of the new provider
    m_hoverManager->setProvider(m_completionProvider);
    m_signatureHelpManager->setProvider(m_completionProvider);
}

void CodeEditor::setCompletionProvider(CompletionProvider *provider)
{
    if (m_completionProvider) {
        disconnect(m_completionProvider, nullptr, this, nullptr);
        disconnect(m_completionProvider, nullptr, m_hoverManager, nullptr);
        disconnect(m_completionProvider, nullptr, m_signatureHelpManager, nullptr);
        if (m_ownsProvider) {
            // Shut down LSP before deleting to prevent use-after-free
            if (auto *cpp = qobject_cast<CppCompletionProvider*>(m_completionProvider))
                cpp->shutdown();
            else if (auto *py = qobject_cast<PythonCompletionProvider*>(m_completionProvider))
                py->shutdown();
            m_completionProvider->deleteLater();
        }
        m_completionProvider = nullptr;
    }

    m_completionProvider = provider;
    m_ownsProvider = false;

    if (provider) {
        connect(provider, &CompletionProvider::completionReady,
                this, &CodeEditor::onCompletionsReady);
        connect(provider, &CompletionProvider::semanticTokensReady,
                this, [this](const QList<SemanticToken> &tokens) {
            if (m_highlighter) {
                if (auto *cppHL = qobject_cast<CppSyntaxHighlighter*>(m_highlighter))
                    cppHL->setSemanticTokens(tokens);
                else if (auto *pyHL = qobject_cast<PythonSyntaxHighlighter*>(m_highlighter))
                    pyHL->setSemanticTokens(tokens);
            }
            emit semanticTokensApplied();
        });
        m_hoverManager->setProvider(provider);
        m_signatureHelpManager->setProvider(provider);
    }
}

void CodeEditor::onProviderFailed(const QString &reason)
{
    qWarning() << "CodeEditor: provider failed:" << reason
               << "— falling back to keyword completion";

    if (m_completionProvider) {
        disconnect(m_completionProvider, nullptr, this, nullptr);
        disconnect(m_completionProvider, nullptr, m_hoverManager, nullptr);
        disconnect(m_completionProvider, nullptr, m_signatureHelpManager, nullptr);
        if (m_ownsProvider) {
            m_completionProvider->deleteLater();
        }
        m_completionProvider = nullptr;
    }

    auto *kw = new KeywordCompletionProvider(m_languageId, this);
    m_completionProvider = kw;
    m_ownsProvider = true;

    connect(m_completionProvider, &CompletionProvider::completionReady,
            this, &CodeEditor::onCompletionsReady);

    m_hoverManager->setProvider(m_completionProvider);
    m_signatureHelpManager->setProvider(m_completionProvider);
}

void CodeEditor::onCompletionsReady(QList<CompletionItem> items)
{
    if (items.isEmpty()) {
        m_completionPopup->hide();
        return;
    }

    // Position popup near the cursor
    QRect cr = cursorRect();
    QPoint pos = viewport()->mapToGlobal(cr.bottomLeft());
    pos.ry() += 2;
    m_completionPopup->move(pos);
    m_completionPopup->showItems(items);
}

// ---- Line number area ----

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(
        QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::refreshLineNumberArea()
{
    updateLineNumberAreaWidth(0);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(
        QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    m_lineNumberArea->update();
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    const auto &cfg = ConfigManager::instance();
    QPainter painter(m_lineNumberArea);
    painter.setFont(font());
    painter.fillRect(event->rect(), m_cachedLnBg);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(m_cachedLnFg);
            painter.drawText(0, top, m_lineNumberArea->width() - cfg.editorLineNumberRightPadding(),
                             fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// ---- Current line highlight ----

void CodeEditor::highlightCurrentLine()
{
    updateExtraSelectionsWithDiagnostics();
}

void CodeEditor::updateExtraSelectionsWithDiagnostics()
{
    QList<QTextEdit::ExtraSelection> extraSelections = m_searchHighlights;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(m_cachedCurrentLine);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    // Add diagnostic squigglies
    for (const auto &diag : m_diagnostics) {
        QTextEdit::ExtraSelection sel;
        QTextCharFormat fmt;
        fmt.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        auto &tm = ThemeManager::instance();
        fmt.setUnderlineColor(diag.severity == 1 ? tm.color("diagnostics.error")
                                                  : tm.color("diagnostics.warning"));
        fmt.setToolTip(diag.message);
        sel.format = fmt;

        QTextBlock startBlock = document()->findBlockByLineNumber(diag.startLine);
        QTextBlock endBlock = document()->findBlockByLineNumber(diag.endLine);
        if (!startBlock.isValid()) continue;
        int startPos = startBlock.position() + diag.startCol;
        int endPos;
        if (endBlock.isValid())
            endPos = endBlock.position() + diag.endCol;
        else
            endPos = startPos;
        sel.cursor = QTextCursor(document());
        sel.cursor.setPosition(startPos);
        sel.cursor.setPosition(endPos, QTextCursor::KeepAnchor);
        extraSelections.append(sel);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::setDiagnostics(const QList<SmdDiagnostic> &diagnostics)
{
    m_diagnostics = diagnostics;
    highlightCurrentLine();
}

void CodeEditor::clearDiagnostics()
{
    m_diagnostics.clear();
    highlightCurrentLine();
}

const SmdDiagnostic* CodeEditor::diagnosticAt(int line, int col) const
{
    for (const auto &d : m_diagnostics) {
        if (line < d.startLine || line > d.endLine)
            continue;
        if (line == d.startLine && col < d.startCol)
            continue;
        if (line == d.endLine && col > d.endCol)
            continue;
        return &d;
    }
    return nullptr;
}

bool CodeEditor::isPositionOverText(const QPoint &viewportPos) const
{
    QTextBlock block = firstVisibleBlock();
    QPointF offset = contentOffset();

    while (block.isValid()) {
        QRectF geo = blockBoundingGeometry(block).translated(offset);

        if (geo.top() > viewportPos.y())
            break;

        if (viewportPos.y() >= geo.top() && viewportPos.y() <= geo.bottom()) {
            QTextLayout *layout = block.layout();
            if (!layout)
                return false;

            for (int i = 0; i < layout->lineCount(); ++i) {
                QTextLine line = layout->lineAt(i);
                qreal lineTop = geo.top() + line.y();
                if (viewportPos.y() >= lineTop && viewportPos.y() <= lineTop + line.height()) {
                    QRectF nr = line.naturalTextRect();
                    return viewportPos.x() <= geo.left() + nr.right();
                }
            }

            return false;
        }

        block = block.next();
    }

    return false;
}

// ---- Key handling ----

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    // Debug: trace Ctrl+D
    if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
        debugLog("CodeEditor::keyPressEvent: Ctrl+D received!");
    }

    // ---- Completion popup key routing ----
    if (m_completionPopup && m_completionPopup->isActive()) {
        // Accept modifiers + alphanumeric: just let through (don't close popup)
        if (event->modifiers() & Qt::ControlModifier) {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        switch (event->key()) {
        case Qt::Key_Up:
            m_completionPopup->selectPrevious();
            return;
        case Qt::Key_Down:
            m_completionPopup->selectNext();
            return;
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            QPlainTextEdit::keyPressEvent(event);
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            insertCompletion(m_completionPopup->selectedItem());
            m_completionPopup->hide();
            return;
        case Qt::Key_Escape:
            m_completionPopup->hide();
            return;
        default:
            // Any other key: close popup, then let normal handling proceed
            m_completionPopup->hide();
            break;
        }
    }

    // ---- Signature help popup key routing ----
    if (m_signatureHelpManager && m_signatureHelpManager->isActive()) {
        switch (event->key()) {
        case Qt::Key_Up:
            m_signatureHelpManager->navigatePrev();
            return;
        case Qt::Key_Down:
            m_signatureHelpManager->navigateNext();
            return;
        case Qt::Key_Escape:
            m_signatureHelpManager->hide();
            return;
        default:
            break;
        }
    }

    // ---- Configurable shortcuts (loaded from settings) ----
    auto matchShortcut = [&](const QKeySequence &seq) -> bool {
        if (seq.isEmpty())
            return false;
        Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
        return QKeySequence(mods | event->key()) == seq;
    };

    if (matchShortcut(m_toggleComment)) {
        handleToggleComment();
        return;
    }

    if (matchShortcut(m_completionTrigger)) {
        triggerCompletion();
        return;
    }

    if (matchShortcut(m_indentRight)) {
        handleIndentRight();
        return;
    }

    if (matchShortcut(m_indentLeft)) {
        handleIndentLeft();
        return;
    }

    if (matchShortcut(m_toggleDiagnostics)) {
        debugLog("CodeEditor: toggleDiagnostics shortcut matched");
        emit diagnosticsToggleRequested();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        handleAutoIndent();
        return;

    case Qt::Key_Tab:
        if (handleTabKey(event))
            return;
        break;

    case Qt::Key_Backspace:
        if (handleBackspaceIndent(event))
            return;
        if (handleBackspacePairRemoval(event))
            return;
        break;

    default:
        break;
    }

    // Bracket completion: opening characters
    QString text = event->text();
    if (!text.isEmpty() && !event->matches(QKeySequence::Paste)) {
        if (text == QStringLiteral("{") || text == QStringLiteral("(") ||
            text == QStringLiteral("[") || text == QStringLiteral("\"") ||
            text == QStringLiteral("'"))
        {
            if (handleBracketCompletion(event))
                return;
        }
        // Closing bracket skip-over
        if (text == QStringLiteral("}") || text == QStringLiteral(")") ||
            text == QStringLiteral("]") || text == QStringLiteral("\"") ||
            text == QStringLiteral("'"))
        {
            if (handleClosingBracketSkip(event))
                return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);

    // After text insertion, check for completion auto-trigger characters
    if (!text.isEmpty() && !event->matches(QKeySequence::Paste)) {
        if (text == QStringLiteral(".")) {
            triggerCompletion();
        } else if (text == QStringLiteral(">")) {
            // -> 成员指针访问才触发，> 单独（模板关括号、iostream>）不触发
            int pos = textCursor().position();
            if (pos >= 2 && document()->characterAt(pos - 2) == QLatin1Char('-'))
                triggerCompletion();
        } else if (text == QStringLiteral(":")) {
            // :: 作用域解析 — 仅当上一个字符也是 : 时触发
            int pos = textCursor().position();
            if (pos >= 2 && document()->characterAt(pos - 2) == QLatin1Char(':'))
                triggerCompletion();
        }
    }
}

// ---- Auto-indent ----

void CodeEditor::handleAutoIndent()
{
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    QString blockText = block.text();
    int posInBlock = cursor.positionInBlock();

    // Extract leading whitespace from current line
    int i = 0;
    while (i < blockText.length() && (blockText.at(i) == QLatin1Char(' ') ||
                                      blockText.at(i) == QLatin1Char('\t'))) {
        ++i;
    }
    QString indent = blockText.left(i);

    // Split {} into three lines when cursor is between them
    if (posInBlock > 0 && posInBlock < blockText.length() &&
        blockText.at(posInBlock - 1) == QLatin1Char('{') &&
        blockText.at(posInBlock) == QLatin1Char('}'))
    {
        QString innerIndent = indent + indentString();
        cursor.beginEditBlock();
        cursor.insertText(QStringLiteral("\n") + innerIndent + QStringLiteral("\n") + indent);
        // Move cursor back to the middle line, at the end of innerIndent
        for (int k = 0; k < indent.length() + 1; ++k)
            cursor.movePosition(QTextCursor::Left);
        cursor.endEditBlock();
        setTextCursor(cursor);
        return;
    }

    // Only add one more indent level if the text before the cursor ends with
    // { (C-style) or : (Python), not just anywhere on the line.
    QString trimmedBefore = blockText.left(posInBlock).trimmed();
    bool pythonColon = (m_languageId == QStringLiteral("python") && trimmedBefore.endsWith(QLatin1Char(':')));
    if (trimmedBefore.endsWith(QLatin1Char('{')) || pythonColon) {
        indent += indentString();
    }
    // If the cursor is before the current indent text, use cursor position for indent
    if (posInBlock < i) {
        indent = blockText.left(posInBlock);
    }

    cursor.beginEditBlock();
    cursor.insertText(QStringLiteral("\n") + indent);
    cursor.endEditBlock();
    setTextCursor(cursor);
}

// ---- Bracket completion ----

bool CodeEditor::handleBracketCompletion(QKeyEvent *event)
{
    if (isCursorInStringOrComment())
        return false;

    QString opening = event->text();
    QChar openChar = opening.at(0);
    QChar closeChar;
    if (openChar == QLatin1Char('{'))      closeChar = QLatin1Char('}');
    else if (openChar == QLatin1Char('(')) closeChar = QLatin1Char(')');
    else if (openChar == QLatin1Char('[')) closeChar = QLatin1Char(']');
    else if (openChar == QLatin1Char('"')) closeChar = QLatin1Char('"');
    else if (openChar == QLatin1Char('\'')) closeChar = QLatin1Char('\'');
    else return false;

    QTextCursor cursor = textCursor();

    if (cursor.hasSelection()) {
        // Wrap selection
        QString sel = cursor.selectedText();
        cursor.beginEditBlock();
        int start = cursor.selectionStart();
        int end = cursor.selectionEnd();
        cursor.clearSelection();
        cursor.setPosition(start);
        cursor.insertText(opening);
        cursor.setPosition(end + 1);
        cursor.insertText(QString(closeChar));
        cursor.endEditBlock();
    } else {
        cursor.beginEditBlock();
        cursor.insertText(opening + QString(closeChar));
        cursor.movePosition(QTextCursor::PreviousCharacter);
        cursor.endEditBlock();
        setTextCursor(cursor);
    }
    return true;
}

// ---- Closing bracket skip ----

bool CodeEditor::handleClosingBracketSkip(QKeyEvent *event)
{
    QString closing = event->text();
    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
        return false;

    if (!cursor.atBlockEnd()) {
        QChar nextChar = document()->characterAt(cursor.position());
        if (nextChar == closing.at(0)) {
            cursor.movePosition(QTextCursor::Right);
            setTextCursor(cursor);
            return true;
        }
    }
    return false;
}

// ---- Backspace pair removal ----

bool CodeEditor::handleBackspaceIndent(QKeyEvent *event)
{
    Q_UNUSED(event);
    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
        return false;

    int pos = cursor.position();
    if (pos == 0)
        return false;

    QTextBlock block = cursor.block();
    int posInBlock = cursor.positionInBlock();
    if (posInBlock == 0)
        return false;

    // Only in leading whitespace
    QString textBeforeCursor = block.text().left(posInBlock);
    if (!textBeforeCursor.trimmed().isEmpty())
        return false;

    if (block.text().at(posInBlock - 1) == QLatin1Char('\t')) {
        cursor.deletePreviousChar();
        return true;
    }

    // Delete spaces back to previous tab stop
    int spaceCount = posInBlock % m_indentWidth;
    if (spaceCount == 0)
        spaceCount = m_indentWidth;

    cursor.beginEditBlock();
    for (int j = 0; j < spaceCount; ++j)
        cursor.deletePreviousChar();
    cursor.endEditBlock();
    return true;
}

bool CodeEditor::handleBackspacePairRemoval(QKeyEvent *event)
{
    Q_UNUSED(event);
    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
        return false;

    int pos = cursor.position();
    if (pos == 0)
        return false;

    QChar leftChar = document()->characterAt(pos - 1);
    QChar expectedRight;

    if (leftChar == QLatin1Char('{'))      expectedRight = QLatin1Char('}');
    else if (leftChar == QLatin1Char('(')) expectedRight = QLatin1Char(')');
    else if (leftChar == QLatin1Char('[')) expectedRight = QLatin1Char(']');
    else if (leftChar == QLatin1Char('"')) expectedRight = QLatin1Char('"');
    else if (leftChar == QLatin1Char('\'')) expectedRight = QLatin1Char('\'');
    else return false;

    if (pos < document()->characterCount() - 1) {
        QChar rightChar = document()->characterAt(pos);
        if (rightChar == expectedRight) {
            cursor.beginEditBlock();
            cursor.deletePreviousChar();
            cursor.deleteChar();
            cursor.endEditBlock();
            return true;
        }
    }
    return false;
}

// ---- Tab handling ----

bool CodeEditor::handleTabKey(QKeyEvent *event)
{
    Q_UNUSED(event);

    QTextCursor cursor = textCursor();

    if (cursor.hasSelection()) {
        // Indent selected lines
        QTextDocument *doc = document();
        int startBlock = doc->findBlock(cursor.selectionStart()).blockNumber();
        int endBlock = doc->findBlock(cursor.selectionEnd()).blockNumber();

        cursor.beginEditBlock();
        for (int i = startBlock; i <= endBlock; ++i) {
            QTextBlock block = doc->findBlockByNumber(i);
            QTextCursor blockCursor(block);
            blockCursor.insertText(indentString());
        }
        cursor.endEditBlock();
    } else {
        cursor.insertText(indentString());
    }
    return true;
}

// ---- Toggle comment ----

QString CodeEditor::commentPrefix() const
{
    if (m_languageId == QStringLiteral("python"))
        return QStringLiteral("#");
    return QStringLiteral("//");
}

void CodeEditor::handleToggleComment()
{
    QTextCursor cursor = textCursor();
    QTextDocument *doc = document();

    int startBlock = doc->findBlock(cursor.selectionStart()).blockNumber();
    int endBlock = doc->findBlock(cursor.selectionEnd()).blockNumber();

    // When the selection ends exactly at column 0 of a subsequent block,
    // exclude that trailing unselected line.
    if (cursor.selectionEnd() > cursor.selectionStart() &&
        doc->findBlock(cursor.selectionEnd()).position() == cursor.selectionEnd() &&
        startBlock != endBlock)
    {
        --endBlock;
    }

    QString prefix = commentPrefix();

    // Check whether all non-blank lines are already commented (prefix at col 0)
    bool allCommented = true;
    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock block = doc->findBlockByNumber(i);
        if (block.text().trimmed().isEmpty())
            continue;
        if (!block.text().startsWith(prefix)) {
            allCommented = false;
            break;
        }
    }

    // ----- Save original selection as (blockNumber, offsetInBlock) -----
    QTextBlock origStartBlock = doc->findBlock(cursor.selectionStart());
    QTextBlock origEndBlock = doc->findBlock(cursor.selectionEnd());
    int origStartBlockNum = origStartBlock.blockNumber();
    int origEndBlockNum = origEndBlock.blockNumber();
    int origStartOffset = cursor.selectionStart() - origStartBlock.position();
    int origEndOffset = cursor.selectionEnd() - origEndBlock.position();

    // ----- Pre-compute per-block text shift -----
    // comment  → shift = +prefix+space  (added at column 0)
    // uncomment → shift = -(prefix+space) or -(prefix) removed from column 0
    QMap<int, int> shiftMap;
    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock block = doc->findBlockByNumber(i);
        QString text = block.text();
        if (text.trimmed().isEmpty())
            continue;

        int shift = 0;
        if (allCommented) {
            if (text.startsWith(prefix)) {
                int removeLen = prefix.length();
                if (removeLen < text.length() && text.at(removeLen) == QLatin1Char(' '))
                    ++removeLen;
                shift = -removeLen;
            }
        } else {
            shift = prefix.length() + 1;
        }
        if (shift != 0)
            shiftMap[i] = shift;
    }

    // ----- Apply edits -----
    cursor.beginEditBlock();

    for (int i = startBlock; i <= endBlock; ++i) {
        if (!shiftMap.contains(i))
            continue;
        QTextBlock block = doc->findBlockByNumber(i);
        QTextCursor lineCursor(block);

        if (allCommented) {
            // Remove prefix (and optional trailing space) from column 0
            QString text = block.text();
            int idx = text.indexOf(prefix, 0);
            if (idx < 0)
                continue;
            int removeLen = prefix.length();
            if (idx + removeLen < text.length() && text.at(idx + removeLen) == QLatin1Char(' '))
                ++removeLen;
            lineCursor.setPosition(block.position() + idx);
            lineCursor.setPosition(block.position() + idx + removeLen, QTextCursor::KeepAnchor);
            lineCursor.removeSelectedText();
        } else {
            // Insert prefix + space at column 0
            lineCursor.setPosition(block.position());
            lineCursor.insertText(prefix + QStringLiteral(" "));
        }
    }

    cursor.endEditBlock();

    // ----- Restore selection (adjust offsets for modified blocks) -----
    QTextCursor newCursor = textCursor();
    QTextBlock newStartBlock = doc->findBlockByNumber(origStartBlockNum);
    QTextBlock newEndBlock = doc->findBlockByNumber(origEndBlockNum);

    int newStartOff = shiftMap.contains(origStartBlockNum)
        ? qMax(0, origStartOffset + shiftMap.value(origStartBlockNum))
        : origStartOffset;
    int newEndOff = shiftMap.contains(origEndBlockNum)
        ? qMax(0, origEndOffset + shiftMap.value(origEndBlockNum))
        : origEndOffset;

    int newStart = newStartBlock.position() + qMin(newStartOff, newStartBlock.length() - 1);
    int newEnd   = newEndBlock.position()   + qMin(newEndOff,   newEndBlock.length() - 1);

    newCursor.setPosition(newStart);
    newCursor.setPosition(newEnd, QTextCursor::KeepAnchor);
    setTextCursor(newCursor);
}

// ---- Indent left/right ----

void CodeEditor::handleIndentRight()
{
    QTextCursor cursor = textCursor();
    QTextDocument *doc = document();
    QString indent = indentString();

    if (cursor.hasSelection()) {
        int startBlock = doc->findBlock(cursor.selectionStart()).blockNumber();
        int endBlock = doc->findBlock(cursor.selectionEnd()).blockNumber();
        // If the selection ends exactly at column 0 of a subsequent block,
        // exclude that trailing unselected line.
        if (cursor.selectionEnd() > cursor.selectionStart()
            && doc->findBlock(cursor.selectionEnd()).position() == cursor.selectionEnd()
            && startBlock != endBlock) {
            --endBlock;
        }
        cursor.beginEditBlock();
        for (int i = startBlock; i <= endBlock; ++i) {
            QTextBlock block = doc->findBlockByNumber(i);
            if (block.text().trimmed().isEmpty())
                continue; // skip empty lines in selection
            QTextCursor lineCursor(block);
            lineCursor.movePosition(QTextCursor::StartOfBlock);
            lineCursor.insertText(indent);
        }
        cursor.endEditBlock();
    } else {
        // No selection: indent current line only
        QTextBlock block = textCursor().block();
        QTextCursor lineCursor(block);
        lineCursor.movePosition(QTextCursor::StartOfBlock);
        lineCursor.insertText(indent);
    }
}

void CodeEditor::handleIndentLeft()
{
    QTextCursor cursor = textCursor();
    QTextDocument *doc = document();

    auto removeLeadingIndent = [this](const QTextBlock &block) -> int {
        QString text = block.text();
        int removeCount = 0;
        for (int j = 0; j < m_indentWidth && j < text.length(); ++j) {
            if (text.at(j) == QLatin1Char(' '))
                ++removeCount;
            else if (text.at(j) == QLatin1Char('\t')) {
                removeCount = j + 1;
                break;
            } else
                break;
        }
        return removeCount;
    };

    if (cursor.hasSelection()) {
        int startBlock = doc->findBlock(cursor.selectionStart()).blockNumber();
        int endBlock = doc->findBlock(cursor.selectionEnd()).blockNumber();
        if (cursor.selectionEnd() > cursor.selectionStart()
            && doc->findBlock(cursor.selectionEnd()).position() == cursor.selectionEnd()
            && startBlock != endBlock) {
            --endBlock;
        }
        cursor.beginEditBlock();
        for (int i = startBlock; i <= endBlock; ++i) {
            QTextBlock block = doc->findBlockByNumber(i);
            if (block.text().trimmed().isEmpty())
                continue; // skip empty lines in selection
            int remove = removeLeadingIndent(block);
            if (remove > 0) {
                QTextCursor lineCursor(block);
                lineCursor.setPosition(block.position() + remove, QTextCursor::KeepAnchor);
                lineCursor.removeSelectedText();
            }
        }
        cursor.endEditBlock();
    } else {
        QTextBlock block = textCursor().block();
        int remove = removeLeadingIndent(block);
        if (remove > 0) {
            QTextCursor lineCursor(block);
            lineCursor.setPosition(block.position() + remove, QTextCursor::KeepAnchor);
            lineCursor.removeSelectedText();
        }
    }
}

// ---- Completion popup helpers ----

void CodeEditor::insertCompletion(const CompletionItem &item)
{
    QString name = item.name.trimmed();
    if (name.isEmpty())
        return;

    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    QString text = toPlainText();

    // Find the start of the current identifier (word boundary backwards)
    int start = pos;
    while (start > 0) {
        QChar c = text.at(start - 1);
        if (c != QLatin1Char('_') && !c.isLetterOrNumber())
            break;
        --start;
    }

    // If there's a partial word, replace it
    if (start < pos) {
        cursor.setPosition(start);
        cursor.setPosition(pos, QTextCursor::KeepAnchor);
    }
    cursor.insertText(name);
    setTextCursor(cursor);
}

bool CodeEditor::eventFilter(QObject *obj, QEvent *event)
{
    // Handle diagnostics toggle shortcut (this or viewport receives ShortcutOverride/KeyPress)
    if ((obj == this || obj == viewport()) && !m_toggleDiagnostics.isEmpty()) {
        if (event->type() == QEvent::ShortcutOverride) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(event);
            Qt::KeyboardModifiers mods = ke->modifiers() & ~Qt::KeypadModifier;
            if (QKeySequence(mods | ke->key()) == m_toggleDiagnostics) {
                debugLog("CodeEditor::eventFilter: ShortcutOverride accepted");
                event->accept();
                return true;
            }
        }
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(event);
            Qt::KeyboardModifiers mods = ke->modifiers() & ~Qt::KeypadModifier;
            if (QKeySequence(mods | ke->key()) == m_toggleDiagnostics) {
                debugLog("CodeEditor::eventFilter: KeyPress -> emit");
                emit diagnosticsToggleRequested();
                return true;
            }
        }
    }

    // Close popup on mouse click outside of it
    if (obj == viewport() && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        QPoint globalPos = me->globalPosition().toPoint();
        if (m_completionPopup && m_completionPopup->isActive()) {
            if (!m_completionPopup->geometry().contains(globalPos)) {
                m_completionPopup->hide();
            }
        }
        if (m_signatureHelpManager && m_signatureHelpManager->isActive()) {
            hideSignatureHelp();
        }
    }
    return QPlainTextEdit::eventFilter(obj, event);
}

void CodeEditor::hideSignatureHelp()
{
    if (m_signatureHelpManager)
        m_signatureHelpManager->hide();
}

void CodeEditor::focusOutEvent(QFocusEvent *event)
{
    hideSignatureHelp();
    QPlainTextEdit::focusOutEvent(event);
}

// ---- Helpers ----

QString CodeEditor::indentString() const
{
    return QString(m_indentWidth, QLatin1Char(' '));
}

bool CodeEditor::isCursorInStringOrComment() const
{
    if (!m_highlighter)
        return false;

    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    if (pos <= 0)
        return false;

    // Check the format at the cursor position.
    // If the cursor is at the end, check the character just before.
    int checkPos = (pos == document()->characterCount() - 1) ? pos - 1 : pos;
    if (checkPos < 0)
        return false;

    QTextBlock block = document()->findBlock(checkPos);
    // Use previousBlockState + highlightBlock to get format
    // Instead, check the actual format list at this position
    const auto &cfg = ConfigManager::instance();
    QVector<QTextLayout::FormatRange> formats = block.layout()->formats();
    int offsetInBlock = checkPos - block.position();
    for (const auto &fmt : formats) {
        if (offsetInBlock >= fmt.start && offsetInBlock < fmt.start + fmt.length) {
            QColor fg = fmt.format.foreground().color();
            if (fg == cfg.syntaxComments() || fg == cfg.syntaxStrings())
                return true;
            break;
        }
    }
    return false;
}

void CodeEditor::setSearchHighlights(const QString &searchText)
{
    m_searchHighlightText = searchText;
    m_searchHighlights.clear();

    if (!searchText.isEmpty()) {
        QTextDocument *doc = document();
        QTextCursor searchCursor(doc);
        while (true) {
            QTextCursor found = doc->find(searchText, searchCursor);
            if (found.isNull())
                break;

            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(ThemeManager::instance().color("search.highlightBackground"));
            sel.format.setForeground(ThemeManager::instance().color("search.highlightForeground"));
            sel.cursor = found;
            m_searchHighlights.append(sel);

            searchCursor = found;
            searchCursor.movePosition(QTextCursor::EndOfWord);
        }
    }

    // Merge with current line highlight
    highlightCurrentLine();
}

void CodeEditor::clearSearchHighlights()
{
    m_searchHighlights.clear();
    m_searchHighlightText.clear();
    highlightCurrentLine();
}

void CodeEditor::clearCurrentLineHighlight()
{
    setExtraSelections(m_searchHighlights);
}

void CodeEditor::refreshCurrentLineHighlight()
{
    highlightCurrentLine();
}

#include "codeeditor.moc"
