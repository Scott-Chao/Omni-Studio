#ifndef TITLEBARBUTTON_H
#define TITLEBARBUTTON_H

#include <QPushButton>

class TitleBarButton : public QPushButton
{
    Q_OBJECT
public:
    enum Type { Minimize, Maximize, Restore, Close };

    explicit TitleBarButton(Type type, QWidget *parent = nullptr);

    void setType(Type type);
    Type buttonType() const { return m_type; }

    QSize sizeHint() const override { return QSize(42, 28); }

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    Type m_type;
    bool m_hovered = false;

    QColor hoverBgColor() const;
    void paintIcon(QPainter &p, const QRect &r, const QColor &fg);
    void paintMinimize(QPainter &p, const QRect &r, const QColor &fg);
    void paintMaximize(QPainter &p, const QRect &r, const QColor &fg);
    void paintRestore(QPainter &p, const QRect &r, const QColor &fg);
    void paintClose(QPainter &p, const QRect &r, const QColor &fg);
};

#endif // TITLEBARBUTTON_H
