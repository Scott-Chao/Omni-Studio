#include "hovermanager.h"
#include "codeeditor.h"

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

    qDebug() << "HoverManager: created, mouseTracking enabled for viewport";
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

    static int eventCount = 0;
    if (event->type() == QEvent::MouseMove) {
        if (++eventCount % 30 == 1) // log roughly every 30th move to avoid spam
            qDebug() << "HoverManager: eventFilter got MouseMove, tooltipShowing=" << m_tooltipShowing;
    }

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

void HoverManager::requestHoverAt(const QPoint &viewportPos)
{
    if (!m_provider) {
        return;
    }

    QTextCursor cursor = m_editor->cursorForPosition(viewportPos);
    m_hoverCursorPos = cursor.position();
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

    qDebug() << "HoverManager: showing tooltip" << text.left(80) << "...";
}

void HoverManager::hideHover()
{
    if (m_tooltipShowing)
        qDebug() << "HoverManager: hiding tooltip";
    QToolTip::hideText();
    m_hoverTimer.stop();
    m_hoverCursorPos = -1;
    m_tooltipShowing = false;
}
