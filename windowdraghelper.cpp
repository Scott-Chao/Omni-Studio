#include "windowdraghelper.h"
#include <QWidget>
#include <QMouseEvent>

bool WindowDragHelper::handlePress(QWidget *widget, QMouseEvent *event, int titleBarHeight)
{
    if (event->button() == Qt::LeftButton && event->position().y() <= titleBarHeight) {
        m_dragging = true;
        m_dragStartPos = event->globalPosition().toPoint();
        m_dragStartGeometry = widget->geometry();
        return true;
    }
    return false;
}

bool WindowDragHelper::handleMove(QWidget *widget, QMouseEvent *event)
{
    if (!m_dragging)
        return false;

    QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
    QRect newGeo = m_dragStartGeometry.translated(delta);

    // Clamp to parent bounds if available
    if (auto *p = widget->parentWidget()) {
        newGeo.setLeft(qMax(0, newGeo.left()));
        newGeo.setTop(qMax(0, newGeo.top()));
        newGeo.setRight(qMin(p->width(), newGeo.right()));
        newGeo.setBottom(qMin(p->height(), newGeo.bottom()));
    }

    widget->move(newGeo.topLeft());
    return true;
}

bool WindowDragHelper::handleRelease(QMouseEvent *event)
{
    Q_UNUSED(event)
    bool was = m_dragging;
    m_dragging = false;
    return was;
}
