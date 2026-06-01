#ifndef WINDOWDRAGHELPER_H
#define WINDOWDRAGHELPER_H

#include <QPoint>
#include <QRect>

class QWidget;
class QMouseEvent;

// Lightweight helper for frameless window drag-to-move.
// Stores drag state (start position, geometry) so widgets don't
// repeat the same press/move/release boilerplate.
// Does NOT inherit QObject — intended as a value member.
class WindowDragHelper
{
public:
    // Returns true if the press started the drag (y ≤ titleBarHeight, left button).
    bool handlePress(QWidget *widget, QMouseEvent *event, int titleBarHeight);
    // Returns true if a drag is in progress (caller should skip default handling).
    bool handleMove(QWidget *widget, QMouseEvent *event);
    // Ends the drag. Returns true if was dragging.
    bool handleRelease(QMouseEvent *event);
    bool isDragging() const { return m_dragging; }

private:
    bool m_dragging = false;
    QPoint m_dragStartPos;
    QRect m_dragStartGeometry;
};

#endif // WINDOWDRAGHELPER_H
