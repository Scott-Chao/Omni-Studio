#include "hovermanager.h"
#include "editor/codeeditor.h"
#include "smd/smddiagnostic.h"
#include "core/thememanager.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolTip>
#include <QVBoxLayout>

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

    // Hide tooltip when app focus leaves the editor
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *) {
        if (m_tooltipShowing && !m_diagnosticTooltipActive)
            hideHover();
    });

    // Refresh tooltip background when theme changes
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() {
        if (m_tooltipWidget)
            refreshTooltipStyle();
    });
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
    // Handle events from the custom tooltip widget
    if (obj == m_tooltipWidget) {
        if (event->type() == QEvent::Enter) {
            m_mouseInTooltip = true;
            return true;
        }
        if (event->type() == QEvent::Leave) {
            m_mouseInTooltip = false;
            hideHover();
            return true;
        }
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *we = static_cast<QWheelEvent *>(event);
            QCoreApplication::sendEvent(m_tooltipScrollArea->viewport(), we);
            return true;
        }
        return QObject::eventFilter(obj, event);
    }

    // Handle events from the editor viewport
    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        m_mousePos = me->pos();

        QTextCursor cursor = m_editor->cursorForPosition(m_mousePos);
        int newPos = cursor.position();

        if (m_tooltipShowing) {
            if (!m_editor->isPositionOverText(m_mousePos)) {
                hideHover();
                return false;
            }
            // Keep tooltip if still within the same identifier / word
            if (isSameWord(newPos, m_hoverCursorPos))
                return false;
            hideHover();
            // fall through to re-arm timer for the new position
        }

        if (!m_editor->isPositionOverText(m_mousePos))
            return false;

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
        if (m_tooltipShowing && !m_diagnosticTooltipActive) {
            // Mouse may have moved onto the tooltip positioned below the text
            if (m_mouseInTooltip) return false;
            if (m_tooltipWidget && m_tooltipWidget->isVisible()
                && m_tooltipWidget->geometry().contains(QCursor::pos()))
                return false;
        }
        hideHover();
        return false;

    case QEvent::MouseButtonPress:
        if (m_tooltipShowing && !m_diagnosticTooltipActive) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_tooltipWidget && m_tooltipWidget->isVisible()
                && m_tooltipWidget->geometry().contains(me->globalPosition().toPoint()))
                return false;
        }
        hideHover();
        return false;

    case QEvent::Wheel:
        if (m_tooltipShowing && !m_diagnosticTooltipActive)
            return false;
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
        "padding: 0px 4px; margin: 0px; "
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

bool HoverManager::isSameWord(int posA, int posB) const
{
    if (posA == posB)
        return true;
    QString text = m_editor->toPlainText();
    if (text.isEmpty())
        return false;
    int len = text.length();
    // Find word boundary (letters, digits, underscores) around posA
    int aStart = posA, aEnd = posA;
    while (aStart > 0 && (text[aStart - 1].isLetterOrNumber() || text[aStart - 1] == '_'))
        --aStart;
    while (aEnd < len && (text[aEnd].isLetterOrNumber() || text[aEnd] == '_'))
        ++aEnd;
    return posB >= aStart && posB <= aEnd;
}

void HoverManager::requestHoverAt(const QPoint &viewportPos)
{
    if (!m_editor->isPositionOverText(viewportPos))
        return;

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

    if (!m_editor->isPositionOverText(m_mousePos))
        return;

    // Guard: cursor may have moved while the request was in flight
    QTextCursor cursor = m_editor->cursorForPosition(m_mousePos);
    if (cursor.position() != m_hoverCursorPos)
        return;

    showHoverToolTip(info);
}

void HoverManager::createTooltipWidget()
{
    m_tooltipWidget = new QFrame(nullptr);
    m_tooltipWidget->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                                    | Qt::WindowStaysOnTopHint
                                    | Qt::WindowDoesNotAcceptFocus);
    m_tooltipWidget->setAttribute(Qt::WA_ShowWithoutActivating);
    m_tooltipWidget->setObjectName(QStringLiteral("hoverTooltip"));
    m_tooltipWidget->setBackgroundRole(QPalette::ToolTipBase);
    m_tooltipWidget->setForegroundRole(QPalette::ToolTipText);
    m_tooltipWidget->setAutoFillBackground(true);

    QVBoxLayout *layout = new QVBoxLayout(m_tooltipWidget);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(0);

    m_tooltipScrollArea = new QScrollArea(m_tooltipWidget);
    m_tooltipScrollArea->setWidgetResizable(false);
    m_tooltipScrollArea->setFrameShape(QFrame::NoFrame);
    m_tooltipScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tooltipScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tooltipScrollArea->setBackgroundRole(QPalette::ToolTipBase);
    m_tooltipScrollArea->setAutoFillBackground(true);
    m_tooltipScrollArea->viewport()->setBackgroundRole(QPalette::ToolTipBase);
    m_tooltipScrollArea->viewport()->setAutoFillBackground(true);

    m_tooltipLabel = new QLabel();
    m_tooltipLabel->setWordWrap(true);
    m_tooltipLabel->setTextFormat(Qt::RichText);
    m_tooltipLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_tooltipLabel->setBackgroundRole(QPalette::ToolTipBase);
    m_tooltipLabel->setForegroundRole(QPalette::ToolTipText);
    m_tooltipLabel->setAutoFillBackground(true);

    m_tooltipScrollArea->setWidget(m_tooltipLabel);
    layout->addWidget(m_tooltipScrollArea);

    m_tooltipWidget->installEventFilter(this);

    refreshTooltipStyle();
}

void HoverManager::refreshTooltipStyle()
{
    if (!m_tooltipWidget)
        return;

    // Read colours directly from the current theme — m_tooltipWidget->palette()
    // may still be stale after QApplication::setPalette() when a stylesheet
    // has already been applied to this widget.
    ThemeManager &tm = ThemeManager::instance();
    QColor bg = tm.color(QStringLiteral("menu.background"));
    QColor fg = tm.color(QStringLiteral("menu.foreground"));
    QColor sep = tm.color(QStringLiteral("menu.separatorColor"));

    if (!bg.isValid()) bg = QColor("#202122");
    if (!fg.isValid()) fg = QColor("#bfbfbf");
    if (!sep.isValid()) sep = bg.darker(130);

    // Stylesheet for the outer frame + scrollbar
    m_tooltipWidget->setStyleSheet(QStringLiteral(
        "#hoverTooltip { background-color: %1; border: 1px solid %2; border-radius: 5px; }"
        "QScrollBar:vertical { width: 8px; }").arg(bg.name(), sep.name()));

    // Propagate palette to child widgets explicitly. When the parent QFrame
    // has a stylesheet, child widgets stop inheriting QApplication::palette()
    // changes automatically — we must push the new palette ourselves.
    QPalette pal;
    pal.setColor(QPalette::ToolTipBase, bg);
    pal.setColor(QPalette::ToolTipText, fg);
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::WindowText, fg);
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Text, fg);
    m_tooltipScrollArea->setPalette(pal);
    m_tooltipScrollArea->viewport()->setPalette(pal);
    m_tooltipLabel->setPalette(pal);
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

    if (!m_tooltipWidget)
        createTooltipWidget();

    // Build HTML with explicit font for accurate QTextDocument measurement
    QString html = QStringLiteral(
        "<html><body style='font-family:\"Consolas\",\"Microsoft YaHei\",sans-serif;"
        "font-size:12px;white-space:pre-wrap;margin:0;padding:0;'>"
        "%1</body></html>").arg(text.toHtmlEscaped());

    // Frame width (capped)
    int maxWidth = qMin(m_editor->viewport()->width() * 2 / 3, 600);
    // Inside the frame: margins 6+6, plus reserve for possible scrollbar
    int contentWidth = maxWidth - 12;

    // Measure rendered height via QTextDocument (reliable with rich text)
    QTextDocument measureDoc;
    measureDoc.setDefaultFont(m_tooltipLabel->font());
    measureDoc.setHtml(html);
    measureDoc.setTextWidth(contentWidth - 8);
    int fullHeight = qCeil(measureDoc.size().height()) + 4; // small breathing room

    bool overflow = fullHeight > kMaxTooltipHeight;
    int scrollH = overflow ? kMaxTooltipHeight : fullHeight;
    int labelW = contentWidth - (overflow ? 10 : 2);

    // Re-measure with final label width
    measureDoc.setTextWidth(labelW);
    fullHeight = qCeil(measureDoc.size().height()) + 4;

    // Set the label content & size
    m_tooltipLabel->setFixedSize(labelW, fullHeight);
    m_tooltipLabel->setText(html);

    // Scroll area: viewport height is capped; label height may exceed it
    m_tooltipScrollArea->setFixedSize(contentWidth, scrollH);

    // Frame: width = maxWidth, height = scroll area + top/bottom margins (4+4)
    m_tooltipWidget->setFixedSize(maxWidth, scrollH + 8);

    // Position below the hovered text line, slightly left of mouse
    QTextCursor posCursor = m_editor->cursorForPosition(m_mousePos);
    QRect lineRect = m_editor->cursorRect(posCursor);
    int tooltipX = m_mousePos.x() - 15;
    QPoint localPos(tooltipX, lineRect.bottom() + 1);

    QPoint globalPos = m_editor->viewport()->mapToGlobal(localPos);
    int w = m_tooltipWidget->width();
    int h = m_tooltipWidget->height();

    // Clamp X within the editor viewport so tooltip doesn't leak outside
    QRect vpGlobal = QRect(m_editor->viewport()->mapToGlobal(QPoint(0, 0)),
                           m_editor->viewport()->size());
    if (globalPos.x() < vpGlobal.left())
        globalPos.setX(vpGlobal.left());
    if (globalPos.x() + w > vpGlobal.right())
        globalPos.setX(vpGlobal.right() - w);

    // If not enough room below, show above the line
    QScreen *screen = m_editor->screen();
    if (screen) {
        QRect screenGeo = screen->availableGeometry();
        if (globalPos.y() + h > screenGeo.bottom()) {
            globalPos = m_editor->viewport()->mapToGlobal(
                QPoint(tooltipX, lineRect.top() - h - 1));
            // Re-clamp X after switching to above
            if (globalPos.x() < vpGlobal.left())
                globalPos.setX(vpGlobal.left());
            if (globalPos.x() + w > vpGlobal.right())
                globalPos.setX(vpGlobal.right() - w);
        }
        if (globalPos.y() < screenGeo.top())
            globalPos.setY(screenGeo.top());
    }

    m_tooltipWidget->move(globalPos);
    m_tooltipWidget->show();

    m_tooltipShowing = true;
    m_diagnosticTooltipActive = false;
}

void HoverManager::hideHover()
{
    if (m_diagnosticTooltipActive || !m_tooltipWidget) {
        QToolTip::hideText();
        if (!m_savedTooltipStyle.isEmpty()) {
            m_editor->setStyleSheet(m_savedTooltipStyle);
            m_savedTooltipStyle.clear();
        }
    }
    if (m_tooltipWidget)
        m_tooltipWidget->hide();
    m_mouseInTooltip = false;
    m_hoverTimer.stop();
    m_hoverCursorPos = -1;
    m_tooltipShowing = false;
    m_diagnosticTooltipActive = false;
}
