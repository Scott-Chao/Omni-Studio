#include "signaturehelpmanager.h"
#include "codeeditor.h"
#include "thememanager.h"

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
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(2);

    // Header: overload navigation
    m_headerLabel = new QLabel(this);
    m_headerLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(m_headerLabel);

    // Signature: rich text with active parameter highlighted
    m_sigLabel = new QLabel(this);
    m_sigLabel->setWordWrap(true);
    m_sigLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_sigLabel->setCursor(Qt::ArrowCursor);
    layout->addWidget(m_sigLabel);

    // Documentation
    m_docLabel = new QLabel(this);
    m_docLabel->setWordWrap(true);
    m_docLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_docLabel->setCursor(Qt::ArrowCursor);
    layout->addWidget(m_docLabel);

    setCursor(Qt::ArrowCursor);

    ThemeManager::watchTheme(this, &SignatureHelpPopup::refreshStyle);
    refreshStyle();
}

void SignatureHelpPopup::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    setStyleSheet(QStringLiteral(
        "SignatureHelpPopup { background: %1; border: 1px solid %2; }"
    ).arg(tm.color("editor.background").name(),
          tm.color("panel.border").name()));

    m_headerLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }"
    ).arg(tm.color("editorLineNumber.foreground").name()));

    m_sigLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 12px; }"
    ).arg(tm.color("editor.foreground").name()));

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

    // Size
    int w = qMin(600, qMax(300, m_sigLabel->sizeHint().width() + 40));
    adjustSize();
    resize(w, height());
}

void SignatureHelpPopup::updatePosition()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    QRect sg = screen->availableGeometry();
    QRect geo = geometry();

    // Position above the text cursor (not mouse cursor).
    CodeEditor *editor = qobject_cast<CodeEditor *>(parentWidget());
    if (editor) {
        QRect cr = editor->cursorRect();
        QPoint cursorGlobal = editor->viewport()->mapToGlobal(cr.topLeft());
        int x = cursorGlobal.x();
        int y = cursorGlobal.y() - geo.height() - 6;  // always above text cursor
        if (y < sg.top())
            y = sg.top();  // clamp to screen top, never go below
        move(x, y);
    }

    // Keep within screen bounds
    geo = geometry();
    if (geo.right() > sg.right()) move(sg.right() - geo.width(), geo.y());
    if (geo.left() < sg.left())   move(sg.left(), geo.y());
    if (geo.top() < sg.top())     move(geo.x(), sg.top());
    if (geo.bottom() > sg.bottom()) move(geo.x(), sg.bottom() - geo.height());
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
