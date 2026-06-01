#ifndef TITLEBARBUTTON_H
#define TITLEBARBUTTON_H

#include <QPushButton>
#include <QStyle>

class TitleBarButton : public QPushButton
{
    Q_OBJECT
public:
    enum Type { Minimize, Maximize, Restore, Close };

    explicit TitleBarButton(Type type, QWidget *parent = nullptr);

    void setType(Type type);
    Type buttonType() const { return m_type; }

    QSize sizeHint() const override { return QSize(46, 32); }

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    Type m_type;
    bool m_hovered = false;

    void updateIcon();
    QStyle::StandardPixmap standardPixmap(Type type) const;
};

#endif // TITLEBARBUTTON_H
