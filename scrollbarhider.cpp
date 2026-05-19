#include "scrollbarhider.h"
#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QEvent>

ScrollbarHider::ScrollbarHider(QObject *parent)
    : QObject(parent)
{
}

void ScrollbarHider::manage(QAbstractScrollArea *area)
{
    if (!area || m_managed.contains(area))
        return;
    m_managed.insert(area);

    // Watch viewport
    if (auto *vp = area->viewport())
        attach(vp, area);

    // Watch vertical scrollbar
    if (auto *sb = area->verticalScrollBar())
        attach(sb, area);

    // Watch horizontal scrollbar
    if (auto *sb = area->horizontalScrollBar())
        attach(sb, area);

    // Create hide timer (one per area)
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(150);
    connect(timer, &QTimer::timeout, this, &ScrollbarHider::onHideTimeout);
    m_areaTimers[area] = timer;

    // Clean up if the area is destroyed before the hider
    connect(area, &QObject::destroyed, this, [this, area]() {
        m_managed.remove(area);
        if (auto *timer = m_areaTimers.take(area)) {
            timer->stop();
            timer->deleteLater();
        }
    });

    // Start hidden
    setScrollbarVisible(area, false);
}

void ScrollbarHider::attach(QObject *target, QAbstractScrollArea *area)
{
    target->installEventFilter(this);
    m_watched[target] = area;
}

bool ScrollbarHider::eventFilter(QObject *obj, QEvent *event)
{
    auto it = m_watched.find(obj);
    if (it == m_watched.end())
        return QObject::eventFilter(obj, event);

    QAbstractScrollArea *area = it.value();
    if (!area)
        return QObject::eventFilter(obj, event);

    if (event->type() == QEvent::Enter) {
        // Cancel hide timer for this area
        if (auto *timer = m_areaTimers.value(area)) {
            timer->stop();
        }
        setScrollbarVisible(area, true);
    } else if (event->type() == QEvent::Leave) {
        // Start hide timer (if not already running)
        if (auto *timer = m_areaTimers.value(area)) {
            if (!timer->isActive())
                timer->start();
        }
    }

    return QObject::eventFilter(obj, event);
}

void ScrollbarHider::onHideTimeout()
{
    auto *timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;

    // Find which area this timer belongs to
    for (auto it = m_areaTimers.begin(); it != m_areaTimers.end(); ++it) {
        if (it.value() == timer) {
            if (QAbstractScrollArea *area = it.key()) {
                setScrollbarVisible(area, false);
            }
            break;
        }
    }
}

void ScrollbarHider::setScrollbarVisible(QAbstractScrollArea *area, bool visible)
{
    QString qss = makeScrollbarQss(visible);

    if (auto *sb = area->verticalScrollBar())
        sb->setStyleSheet(qss);
    if (auto *sb = area->horizontalScrollBar())
        sb->setStyleSheet(qss);
}

QString ScrollbarHider::makeScrollbarQss(bool visible) const
{
    QString track = visible ? QStringLiteral("#1E1E1E") : QStringLiteral("transparent");
    QString handle = visible ? QStringLiteral("#555555") : QStringLiteral("transparent");

    return QStringLiteral(
        "QScrollBar:vertical {"
        "  background-color: %1;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: %2;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
        "QScrollBar:horizontal {"
        "  background-color: %1;"
        "  height: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background-color: %2;"
        "  min-width: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "  width: 0;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "  background: none;"
        "}"
    ).arg(track, handle);
}
