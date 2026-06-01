#include "signaturehelpmanager.h"
#include "editor/codeeditor.h"
#include "core/thememanager.h"
#include "core/utilities.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextDocument>
#include <QTextCursor>
#include <QScreen>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QFont>
#include <QApplication>

// ============================================================
// SignatureHelpPopup
// ============================================================

SignatureHelpPopup::SignatureHelpPopup(QWidget *parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setObjectName(QStringLiteral("signatureHelpPopup"));
    setBackgroundRole(QPalette::ToolTipBase);
    setForegroundRole(QPalette::ToolTipText);
    setAutoFillBackground(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(2);

    // Header: overload navigation (sticky, always visible)
    m_headerLabel = new QLabel(this);
    m_headerLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(m_headerLabel);

    // Scroll area for signature + documentation (capped height with scrollbar)
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setBackgroundRole(QPalette::ToolTipBase);
    m_scrollArea->setAutoFillBackground(true);
    m_scrollArea->viewport()->setBackgroundRole(QPalette::ToolTipBase);
    m_scrollArea->viewport()->setAutoFillBackground(true);
    layout->addWidget(m_scrollArea);

    m_scrollContent = new QWidget();
    auto *scrollLayout = new QVBoxLayout(m_scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(2);

    // Signature: rich text with active parameter highlighted
    m_sigLabel = new QLabel();
    m_sigLabel->setWordWrap(true);
    m_sigLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_sigLabel->setCursor(Qt::ArrowCursor);
    m_sigLabel->setBackgroundRole(QPalette::ToolTipBase);
    m_sigLabel->setForegroundRole(QPalette::ToolTipText);
    m_sigLabel->setAutoFillBackground(true);
    scrollLayout->addWidget(m_sigLabel);

    // Documentation
    m_docLabel = new QLabel();
    m_docLabel->setWordWrap(true);
    m_docLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_docLabel->setCursor(Qt::ArrowCursor);
    m_docLabel->setBackgroundRole(QPalette::ToolTipBase);
    m_docLabel->setForegroundRole(QPalette::ToolTipText);
    m_docLabel->setAutoFillBackground(true);
    scrollLayout->addWidget(m_docLabel);

    m_scrollArea->setWidget(m_scrollContent);

    setCursor(Qt::ArrowCursor);

    ThemeManager::watchTheme(this, &SignatureHelpPopup::refreshStyle);
    refreshStyle();
}

void SignatureHelpPopup::refreshStyle()
{
    auto &tm = ThemeManager::instance();
    QColor bg = tm.color("menu.background");
    QColor fg = tm.color("menu.foreground");

    if (!bg.isValid()) bg = QColor("#202122");
    if (!fg.isValid()) fg = QColor("#bfbfbf");

    // Compute a border color that's clearly visible against the background.
    // The theme's menu.separatorColor is often too subtle (especially in
    // light mode), so enforce a minimum lightness contrast of 60 steps.
    QColor border = tm.color("menu.separatorColor");
    if (!border.isValid() || qAbs(bg.lightness() - border.lightness()) < 60)
        border = bg.lightness() > 128 ? bg.darker(350) : bg.lighter(350);

    // Stylesheet for the outer frame + scrollbar (matches HoverManager pattern)
    setStyleSheet(QStringLiteral(
        "#signatureHelpPopup { background-color: %1; border: 1px solid %2; border-radius: 5px; }"
        "QScrollBar:vertical { width: 8px; }"
    ).arg(bg.name(), border.name()));

    // Propagate palette to child widgets. When the parent QFrame has a
    // stylesheet, child widgets stop inheriting QApplication::palette()
    // automatically — we must push the new palette ourselves.
    QPalette pal;
    pal.setColor(QPalette::ToolTipBase, bg);
    pal.setColor(QPalette::ToolTipText, fg);
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::WindowText, fg);
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Text, fg);
    m_scrollArea->setPalette(pal);
    m_scrollArea->viewport()->setPalette(pal);
    m_sigLabel->setPalette(pal);
    m_docLabel->setPalette(pal);

    // Foreground colors for header, sig, and doc labels (distinct roles)
    m_headerLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }"
    ).arg(tm.color("editorLineNumber.foreground").name()));

    // Sig label: use editor.foreground for code-like appearance
    m_sigLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 12px; }"
    ).arg(tm.color("editor.foreground").name()));

    // Doc label: muted color for secondary text
    m_docLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }"
    ).arg(tm.color("tab.inactiveForeground").name()));
}

void SignatureHelpPopup::showSignatures(const QList<SignatureInfo> &signatures, int activeIndex)
{
    m_signatures = signatures;
    m_activeIndex = qBound(0, activeIndex, m_signatures.size() - 1);

    updateContent();
    updatePosition();

    show();
    raise();
}

void SignatureHelpPopup::navigateNext()
{
    if (m_signatures.size() <= 1)
        return;
    m_activeIndex = (m_activeIndex + 1) % m_signatures.size();
    updateContent();
}

void SignatureHelpPopup::navigatePrev()
{
    if (m_signatures.size() <= 1)
        return;
    m_activeIndex = (m_activeIndex - 1 + m_signatures.size()) % m_signatures.size();
    updateContent();
}

void SignatureHelpPopup::updateContent()
{
    if (m_activeIndex < 0 || m_activeIndex >= m_signatures.size())
        return;

    const SignatureInfo &sig = m_signatures.at(m_activeIndex);

    // Overload counter
    if (m_signatures.size() > 1) {
        m_headerLabel->setText(
            QStringLiteral("◀ %1 / %2 ▶").arg(m_activeIndex + 1).arg(m_signatures.size()));
        m_headerLabel->show();
    } else {
        m_headerLabel->hide();
    }

    // Signature with active parameter highlighted
    QString sigHtml;
    QString labelText = sig.label.toHtmlEscaped();

    QColor activeParamColor = ThemeManager::instance().color("syntax.keywords");
    if (!sig.parameters.isEmpty() && sig.activeParameter >= 0
        && sig.activeParameter < sig.parameters.size())
    {
        // Find the parameter text in the label and wrap it with <b> tags
        QString paramText = sig.parameters.at(sig.activeParameter);
        QString escapedParam = paramText.toHtmlEscaped();
        int idx = labelText.indexOf(escapedParam);
        if (idx >= 0) {
            sigHtml = labelText.left(idx)
                      + QStringLiteral("<b style=\"color:%1;\">")
                      + escapedParam
                      + QStringLiteral("</b>")
                      + labelText.mid(idx + escapedParam.length());
        } else {
            sigHtml = labelText;
        }
    } else {
        sigHtml = labelText;
    }

    m_sigLabel->setText(QStringLiteral("<span style=\"font-family:Consolas,monospace;\">%1</span>")
                            .arg(sigHtml));

    // Documentation
    if (!sig.doc.isEmpty()) {
        m_docLabel->setText(sig.doc);
        m_docLabel->show();
    } else {
        m_docLabel->hide();
    }

    // Size — cap height with scrollbar for long content
    CodeEditor *editor = qobject_cast<CodeEditor *>(parentWidget());
    int maxWidth = 600;
    if (editor)
        maxWidth = qMin(editor->viewport()->width() * 2 / 3, 600);
    int contentWidth = maxWidth - 16; // margins 8+8

    m_sigLabel->setFixedWidth(contentWidth);
    m_docLabel->setFixedWidth(contentWidth);
    m_scrollContent->setFixedWidth(contentWidth);

    m_scrollContent->adjustSize();

    int scrollContentH = m_scrollContent->height();
    int scrollH = qMin(scrollContentH, kMaxPopupHeight);
    m_scrollArea->setFixedSize(contentWidth, scrollH);

    int headerH = m_headerLabel->isHidden() ? 0 : m_headerLabel->sizeHint().height();
    int spacing = 2;
    int totalH = 12 + headerH + (headerH > 0 ? spacing : 0) + scrollH;
    setFixedSize(maxWidth, totalH);
}

void SignatureHelpPopup::updatePosition()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    QRect sg = screen->availableGeometry();
    QRect geo = geometry();

    // Position near the text cursor (not mouse cursor).
    // Prefer below the cursor line so it doesn't block what the user is typing;
    // fall back to above if insufficient room below.
    CodeEditor *editor = qobject_cast<CodeEditor *>(parentWidget());
    if (editor) {
        QRect cr = editor->cursorRect();
        QPoint cursorGlobal = editor->viewport()->mapToGlobal(cr.topLeft());
        int x = cursorGlobal.x();

        int cursorBottom = cursorGlobal.y() + cr.height();
        int yBelow = cursorBottom + 4;   // 4px gap below cursor line
        int yAbove = cursorGlobal.y() - geo.height() - 6;  // above cursor line

        int y;
        if (yBelow + geo.height() <= sg.bottom()) {
            y = yBelow;     // enough room below — place below
        } else if (yAbove >= sg.top()) {
            y = yAbove;     // not enough below — place above
        } else {
            y = qMax(sg.top(), yBelow);
            if (y + geo.height() > sg.bottom())
                y = sg.bottom() - geo.height();
        }
        move(x, y);
    }

    // Keep within screen bounds
    ScreenUtils::clampToScreen(this);
}

void SignatureHelpPopup::mouseReleaseEvent(QMouseEvent *event)
{
    // Click on the header ◀/▶ areas to navigate overloads
    if (event->button() == Qt::LeftButton && m_signatures.size() > 1) {
        int hdrWidth = m_headerLabel->width();
        int clickX = event->pos().x() - m_headerLabel->x();
        if (clickX >= 0 && clickX <= hdrWidth) {
            // ◀ area: first ~20px
            if (clickX < hdrWidth / 2)
                navigatePrev();
            else
                navigateNext();
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void SignatureHelpPopup::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ============================================================
// SignatureHelpManager
// ============================================================

SignatureHelpManager::SignatureHelpManager(CodeEditor *editor,
                                           CompletionProvider *provider,
                                           QObject *parent)
    : QObject(parent)
    , m_editor(editor)
    , m_provider(provider)
{
    m_popup = new SignatureHelpPopup(m_editor);
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(200); // 200ms debounce for re-request on cursor move

    connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
            this, &SignatureHelpManager::onCursorPositionChanged);

    if (m_provider) {
        connect(m_provider, &CompletionProvider::signatureHelpReady,
                this, &SignatureHelpManager::onSignatureHelpReady);
    }

    connect(&m_debounceTimer, &QTimer::timeout,
            this, &SignatureHelpManager::requestSignature);

}

void SignatureHelpManager::setProvider(CompletionProvider *provider)
{
    if (m_provider)
        disconnect(m_provider, nullptr, this, nullptr);
    m_provider = provider;
    if (m_provider) {
        connect(m_provider, &CompletionProvider::signatureHelpReady,
                this, &SignatureHelpManager::onSignatureHelpReady);
    }
}

void SignatureHelpManager::requestSignature()
{
    if (!m_provider)
        return;

    QTextCursor cursor = m_editor->textCursor();
    m_openParenPos = cursor.position() - 1;

    QString text = m_editor->toPlainText();
    int cursorPos = cursor.position();

    // Strip the auto-inserted ')' that bracket completion placed right after
    // the cursor. Without this, clangd sees e.g. "substr(1,)" instead of
    // "substr(1," and may return empty signatures (interpreting ')' as call close).
    if (cursorPos < text.length() && text.at(cursorPos) == QLatin1Char(')')) {
        text.remove(cursorPos, 1);
    }

    m_provider->requestSignatureHelp(text, cursorPos);
}

void SignatureHelpManager::onCursorPositionChanged()
{
    QTextCursor cursor = m_editor->textCursor();
    int pos = cursor.position();

    if (pos <= 0) {
        if (m_popup->isActive())
            hide();
        return;
    }

    QChar charBefore = m_editor->document()->characterAt(pos - 1);

    // If popup is already visible
    if (m_popup->isActive()) {
        // Close on ')' or if cursor moved before the opening '('
        if (charBefore == QLatin1Char(')') || pos <= m_openParenPos) {
            hide();
            return;
        }
        // Still inside parameter area — debounce re-request to update activeParameter
        m_debounceTimer.start();
        return;
    }

    // Trigger: char before cursor is '('
    if (charBefore == QLatin1Char('(')) {
        requestSignature();
    }
}

void SignatureHelpManager::onSignatureHelpReady(QList<SignatureInfo> signatures, int activeIndex)
{
    // Guard: popup was explicitly closed while request was in flight
    if (m_openParenPos < 0)
        return;

    if (signatures.isEmpty()) {
        // Don't close — the auto-inserted ')' from bracket completion may still
        // be confusing clangd. Keep the last valid signature visible; the popup
        // will close via the normal paths (user types ')', Esc, or cursor < '(').
        return;
    }

    m_popup->showSignatures(signatures, activeIndex);
}

void SignatureHelpManager::navigateNext()
{
    if (m_popup)
        m_popup->navigateNext();
}

void SignatureHelpManager::navigatePrev()
{
    if (m_popup)
        m_popup->navigatePrev();
}

void SignatureHelpManager::hide()
{
    m_debounceTimer.stop();
    if (m_popup && m_popup->isVisible()) {
        m_popup->hide();
    }
    m_openParenPos = -1;
}
