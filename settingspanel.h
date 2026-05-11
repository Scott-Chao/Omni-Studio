#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSlider;
class QVBoxLayout;
class QSizeGrip;

class SettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    QVBoxLayout *contentLayout() const;
    QLabel *titleLabel() const;

    void setDefaultZoom(qreal zoom);
    qreal defaultZoom() const;

signals:
    void closeRequested();
    void defaultZoomChanged(qreal zoom);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;

private:
    enum class Edge {
        None,
        Top, Bottom, Left, Right,
        TopLeft, TopRight, BottomLeft, BottomRight
    };

    static constexpr int kEdgeMargin = 8;
    static constexpr int kTitleBarHeight = 36;

    Edge detectEdge(const QPoint &pos) const;
    void updateCursorForEdge(Edge edge);

    QLabel *m_titleLabel;
    QPushButton *m_closeButton;
    QScrollArea *m_scrollArea;
    QVBoxLayout *m_contentLayout;
    QSizeGrip *m_sizeGrip;
    QSlider *m_fontSizeSlider = nullptr;
    QLineEdit *m_fontSizeEdit = nullptr;

    bool m_dragging = false;
    Edge m_dragEdge = Edge::None;
    QPoint m_dragStartPos;
    QRect m_dragStartGeometry;

    int m_minWidth = 300;
    int m_minHeight = 200;
};

#endif // SETTINGSPANEL_H
