#include "titlebarbutton.h"
#include "thememanager.h"
#include <QPainter>
#include <QEnterEvent>

TitleBarButton::TitleBarButton(Type type, QWidget *parent)
    : QPushButton(parent), m_type(type)
{
    setFixedSize(42, 28);
    setFlat(true);
    setCursor(Qt::ArrowCursor);
    setStyleSheet(QStringLiteral("QPushButton { border: none; background: transparent; }"));
    setMouseTracking(true);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() { update(); });
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

    // Icon color — match activity bar icons
    QColor fg = ThemeManager::instance().color(QStringLiteral("activityBar.foreground"));
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
    QPen pen(fg, 1.0);
    p.setPen(pen);
    qreal cx = r.center().x();
    qreal cy = r.center().y();
    p.drawLine(QPointF(cx - 5.5, cy + 0.5), QPointF(cx + 5.5, cy + 0.5));
}

void TitleBarButton::paintMaximize(QPainter &p, const QRect &r, const QColor &fg)
{
    QPen pen(fg, 1.0);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    qreal cx = r.center().x();
    qreal cy = r.center().y();
    p.drawRect(QRectF(cx - 5.5, cy - 5.0, 11, 10));
}

void TitleBarButton::paintRestore(QPainter &p, const QRect &r, const QColor &fg)
{
    QPen pen(fg, 1.0);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    qreal cx = r.center().x();
    qreal cy = r.center().y();
    // Back square (upper-left)
    p.drawRect(QRectF(cx - 5.0, cy - 5.0, 8, 7));
    // Front square (lower-right)
    p.drawRect(QRectF(cx - 2.0, cy - 2.0, 8, 7));
}

void TitleBarButton::paintClose(QPainter &p, const QRect &r, const QColor &fg)
{
    QPen pen(fg, 1.2);
    p.setPen(pen);
    qreal cx = r.center().x();
    qreal cy = r.center().y();
    p.drawLine(QPointF(cx - 4.5, cy - 4.5), QPointF(cx + 4.5, cy + 4.5));
    p.drawLine(QPointF(cx + 4.5, cy - 4.5), QPointF(cx - 4.5, cy + 4.5));
}
