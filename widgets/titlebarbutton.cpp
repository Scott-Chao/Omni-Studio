#include "titlebarbutton.h"
#include "core/thememanager.h"
#include <QPainter>
#include <QEnterEvent>
#include <QStyleFactory>

TitleBarButton::TitleBarButton(Type type, QWidget *parent)
    : QPushButton(parent), m_type(type)
{
    setFixedSize(46, 32);
    setFlat(true);
    setCursor(Qt::ArrowCursor);
    setStyleSheet(QStringLiteral("QPushButton { border: none; background: transparent; }"));
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    updateIcon();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() { update(); });
}

void TitleBarButton::setType(Type type)
{
    if (m_type != type) {
        m_type = type;
        updateIcon();
    }
}

QStyle::StandardPixmap TitleBarButton::standardPixmap(Type type) const
{
    switch (type) {
    case Minimize: return QStyle::SP_TitleBarMinButton;
    case Maximize: return QStyle::SP_TitleBarMaxButton;
    case Restore:  return QStyle::SP_TitleBarNormalButton;
    case Close:    return QStyle::SP_TitleBarCloseButton;
    }
    return QStyle::SP_TitleBarMinButton;
}

void TitleBarButton::updateIcon()
{
    // Use native Windows style for icons, independent of the app's Fusion style
    static QStyle *s_nativeStyle = QStyleFactory::create(QStringLiteral("windowsvista"));
    setIcon(s_nativeStyle->standardIcon(standardPixmap(m_type)));
    if (m_type == Minimize)
        setIconSize(QSize(28, 28));
    else
        setIconSize(QSize(16, 16));
}

void TitleBarButton::enterEvent(QEnterEvent *) { m_hovered = true; repaint(); }
void TitleBarButton::leaveEvent(QEvent *)      { m_hovered = false; repaint(); }

void TitleBarButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_hovered) {
        QColor bg = (m_type == Close)
            ? QColor(0xc4, 0x2b, 0x1c)
            : ThemeManager::instance().color(QStringLiteral("titleBar.buttonHover"));
        if (!bg.isValid())
            bg = QColor(0x3a, 0x3a, 0x3a);
        p.fillRect(rect(), bg);
    }

    // Draw icon directly to avoid Fusion style frames
    QIcon icon = this->icon();
    QSize sz = iconSize();
    int x = (width() - sz.width()) / 2;
    int y = (height() - sz.height()) / 2;
    icon.paint(&p, x, y, sz.width(), sz.height());
}
