#include "hovermanager.h"
#include "codeeditor.h"
#include "smdlspmanager.h"

#include <QMouseEvent>
#include <QToolTip>
#include <QTextCursor>
#include <QDebug>

HoverManager::HoverManager(CodeEditor *editor, CompletionProvider *provider, QObject *parent)
    : QObject(parent)
    , m_editor(editor)
    , m_provider(provider)
{
    m_hoverTimer.setSingleShot(true);
    m_hoverTimer.setInterval(400); // 400ms delay before hover fires

    connect(&m_hoverTimer, &QTimer::timeout, this, &HoverManager::onHoverTimeout);

    if (m_provider) {
        connect(m_provider, &CompletionProvider::hoverReady,
                this, &HoverManager::onHoverReady);
    }

    if (m_editor && m_editor->viewport()) {
        m_editor->viewport()->setMouseTracking(true);  // required to receive MouseMove events
        m_editor->viewport()->installEventFilter(this);
    }

}

void HoverManager::setProvider(CompletionProvider *provider)
{
    if (m_provider)
        disconnect(m_provider, nullptr, this, nullptr);
    m_provider = provider;
    if (m_provider) {
        connect(m_provider, &CompletionProvider::hoverReady,
                this, &HoverManager::onHoverReady);
    }
}

bool HoverManager::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        m_mousePos = me->pos();

        QTextCursor cursor = m_editor->cursorForPosition(m_mousePos);
        int newPos = cursor.position();

        if (m_tooltipShowing) {
            if (newPos == m_hoverCursorPos)
                return false; // still on the same symbol, keep tooltip
            hideHover();
            // fall through to re-arm timer for the new position
        }

        // Ctrl held → bypass the 400ms delay
        if (me->modifiers() & Qt::ControlModifier) {
            m_hoverTimer.stop();
            requestHoverAt(m_mousePos);
            return false;
        }

        m_hoverTimer.start();
        return false;
    }

    case QEvent::Leave:
        hideHover();
        return false;

    case QEvent::MouseButtonPress:
        hideHover();
        return false;

    case QEvent::Wheel:
        if (m_tooltipShowing)
            hideHover();
        return false;

    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}

void HoverManager::onHoverTimeout()
{
    requestHoverAt(m_mousePos);
}

bool HoverManager::tryShowDiagnosticToolTip(const QPoint &viewportPos)
{
    QTextCursor cursor = m_editor->cursorForPosition(viewportPos);
    int line = cursor.blockNumber();
    int col = cursor.positionInBlock();

    const SmdDiagnostic *diag = m_editor->diagnosticAt(line, col);
    if (!diag)
        return false;

    bool isError = (diag->severity == 1);
    QString severityLabel = isError ? QStringLiteral("错误") : QStringLiteral("警告");
    QString bgColor   = isError ? QStringLiteral("#5a1d1d") : QStringLiteral("#5a4d00");
    QString borderColor = isError ? QStringLiteral("#F44747") : QStringLiteral("#CCA700");
    QString textColor = isError ? QStringLiteral("#f8d7da") : QStringLiteral("#fff3cd");

    // Save & apply tooltip styling so the coloured background fills the
    // entire tooltip window (no black edges).
    m_savedTooltipStyle = m_editor->styleSheet();
    m_editor->setStyleSheet(m_savedTooltipStyle + QStringLiteral(
        " QToolTip { background-color: %1; color: %2; "
        "border: 1px solid %3; border-radius: 3px; "
        "padding: 5px 8px; "
        "font-family: Consolas, Microsoft YaHei, sans-serif; "
        "font-size: 12px; }")
        .arg(bgColor, textColor, borderColor));

    QString html = QStringLiteral(
        "<b>%1:</b> %2")
        .arg(severityLabel, diag->message.toHtmlEscaped());

    QPoint globalPos = m_editor->viewport()->mapToGlobal(viewportPos);
    globalPos += QPoint(15, 20);
    QToolTip::showText(globalPos, html, m_editor);
    m_tooltipShowing = true;
    m_diagnosticTooltipActive = true;
    return true;
}

void HoverManager::requestHoverAt(const QPoint &viewportPos)
{
    QTextCursor cursor = m_editor->cursorForPosition(viewportPos);
    m_hoverCursorPos = cursor.position();

    // Phase 1: Check for diagnostics at cursor position (local data, no round-trip).
    if (tryShowDiagnosticToolTip(viewportPos))
        return;

    // Phase 2: Fall through to LSP hover
    if (!m_provider)
        return;
    QString text = m_editor->toPlainText();
    m_provider->requestHover(text, m_hoverCursorPos);
}

void HoverManager::onHoverReady(HoverInfo info)
{
    if (info.signature.isEmpty() && info.doc.isEmpty())
        return;

    // Guard: cursor may have moved while the request was in flight
    QTextCursor cursor = m_editor->cursorForPosition(m_mousePos);
    if (cursor.position() != m_hoverCursorPos)
        return;

    showHoverToolTip(info);
}

void HoverManager::showHoverToolTip(const HoverInfo &info)
{
    QString text;
    if (!info.signature.isEmpty() && !info.doc.isEmpty()) {
        text = info.signature + QStringLiteral("\n\n") + info.doc;
    } else if (!info.signature.isEmpty()) {
        text = info.signature;
    } else if (!info.doc.isEmpty()) {
        text = info.doc;
    } else {
        return;
    }

    QPoint globalPos = m_editor->viewport()->mapToGlobal(m_mousePos);
    globalPos += QPoint(15, 20); // offset so the tooltip doesn't cover the code
    QToolTip::showText(globalPos, text, m_editor);
    m_tooltipShowing = true;

    qDebug() << "HoverManager: showing tooltip";
}

void HoverManager::hideHover()
{
    QToolTip::hideText();
    m_hoverTimer.stop();
    m_hoverCursorPos = -1;
    m_tooltipShowing = false;
    if (m_diagnosticTooltipActive) {
        m_editor->setStyleSheet(m_savedTooltipStyle);
        m_savedTooltipStyle.clear();
    }
    m_diagnosticTooltipActive = false;
}
