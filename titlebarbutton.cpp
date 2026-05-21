#include "titlebarbutton.h"
#include "thememanager.h"
#include <QPainter>
#include <QEnterEvent>

TitleBarButton::TitleBarButton(Type type, QWidget *parent)
    : QPushButton(parent), m_type(type)
{
    setFixedSize(46, 32);
    setFlat(true);
    setCursor(Qt::ArrowCursor);
    setStyleSheet(QStringLiteral("QPushButton { border: none; background: transparent; }"));
    setMouseTracking(true);
}

void TitleBarButton::setType(Type type)
{
    if (m_type != type) {
        m_type = type;
        update();
    }
}

QColor TitleBarButton::hoverBgColor() const
{
    if (m_type == Close)
        return ThemeManager::instance().color(QStringLiteral("titleBar.buttonCloseHover"));
    return ThemeManager::instance().color(QStringLiteral("titleBar.buttonHover"));
}

void TitleBarButton::enterEvent(QEnterEvent *) { m_hovered = true; repaint(); }
void TitleBarButton::leaveEvent(QEvent *)      { m_hovered = false; repaint(); }

void TitleBarButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Hover background
    if (m_hovered) {
        p.fillRect(rect(), hoverBgColor());
    }

    // Icon color
    QColor fg = ThemeManager::instance().color(QStringLiteral("titleBar.foreground"));
    if (!fg.isValid())
        fg = QColor(QStringLiteral("#CCCCCC"));

    paintIcon(p, rect(), fg);
}

void TitleBarButton::paintIcon(QPainter &p, const QRect &r, const QColor &fg)
{
    switch (m_type) {
    case Minimize: paintMinimize(p, r, fg); break;
    case Maximize: paintMaximize(p, r, fg); break;
    case Restore:  paintRestore(p, r, fg);  break;
    case Close:    paintClose(p, r, fg);    break;
    }
}

void TitleBarButton::paintMinimize(QPainter &p, const QRect &r, const QColor &fg)
{
    QPen pen(fg, 1.5);
    p.setPen(pen);
    qreal y = r.center().y() + 0.5;
    p.drawLine(QPointF(r.left() + 13, y), QPointF(r.right() - 13, y));
}

void TitleBarButton::paintMaximize(QPainter &p, const QRect &r, const QColor &fg)
{
    QPen pen(fg, 1.5);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(r.left() + 13, r.top() + 8, 20, 16), 2, 2);
}

void TitleBarButton::paintRestore(QPainter &p, const QRect &r, const QColor &fg)
{
    QPen pen(fg, 1.5);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // Back square (upper-left)
    p.drawRoundedRect(QRectF(r.left() + 11, r.top() + 6, 14, 11), 1.5, 1.5);
    // Front square (lower-right)
    p.drawRoundedRect(QRectF(r.left() + 16, r.top() + 11, 14, 11), 1.5, 1.5);
}

void TitleBarButton::paintClose(QPainter &p, const QRect &r, const QColor &fg)
{
    QPen pen(fg, 1.8);
    p.setPen(pen);
    qreal cx = r.center().x();
    qreal cy = r.center().y();
    p.drawLine(QPointF(cx - 6, cy - 6), QPointF(cx + 6, cy + 6));
    p.drawLine(QPointF(cx + 6, cy - 6), QPointF(cx - 6, cy + 6));
}
