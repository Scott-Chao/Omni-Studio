#include "settingspanel.h"
#include "configmanager.h"

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QApplication>
#include <QSizeGrip>
#include <QSlider>
#include <QLineEdit>
#include <QIntValidator>

namespace {

class ZoomValidator : public QIntValidator
{
public:
    using QIntValidator::QIntValidator;
    State validate(QString &input, int &pos) const override
    {
        if (input.isEmpty()) return Acceptable;
        return QIntValidator::validate(input, pos);
    }
};

} // namespace

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    const auto &cfg = ConfigManager::instance();
    int panelWidth = cfg.settingsPanelWidth();
    int panelHeight = cfg.settingsPanelHeight();
    m_minWidth = cfg.settingsPanelMinWidth();
    m_minHeight = cfg.settingsPanelMinHeight();

    resize(panelWidth, panelHeight);
    setObjectName("settingsPanel");
    setStyleSheet(
        "#settingsPanel {"
        "  background-color: #2b2b2b;"
        "  border: 1px solid #555555;"
        "  border-radius: 8px;"
        "}"
    );

    setMouseTracking(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- Title bar ----
    auto *titleBar = new QWidget;
    titleBar->setFixedHeight(kTitleBarHeight);
    titleBar->setObjectName("settingsTitleBar");
    titleBar->setStyleSheet(
        "#settingsTitleBar {"
        "  background-color: #333333;"
        "  border-top-left-radius: 8px;"
        "  border-top-right-radius: 8px;"
        "}"
    );

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 8, 0);

    m_titleLabel = new QLabel(tr("设置"));
    m_titleLabel->setStyleSheet("color: #cccccc; font-size: 13px; font-weight: bold;");

    m_closeButton = new QPushButton(QStringLiteral("✕"));
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setFlat(true);
    m_closeButton->setCursor(Qt::ArrowCursor);
    m_closeButton->setStyleSheet(
        "QPushButton { color: #aaaaaa; border: none; font-size: 14px; }"
        "QPushButton:hover { color: #ffffff; background-color: #c42b1c; border-radius: 4px; }"
    );
    connect(m_closeButton, &QPushButton::clicked, this, &SettingsPanel::closeRequested);

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_closeButton);

    // ---- Content area ----
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    );

    auto *scrollContent = new QWidget;
    m_contentLayout = new QVBoxLayout(scrollContent);
    m_contentLayout->setContentsMargins(16, 12, 16, 12);
    m_contentLayout->setSpacing(8);

    // ---- 默认字体大小 ----
    auto *zoomRow = new QHBoxLayout;
    auto *zoomLabel = new QLabel(tr("默认字体大小"));
    zoomLabel->setStyleSheet("color: #cccccc; font-size: 12px;");
    zoomRow->addWidget(zoomLabel);

    int sliderMin = qRound(cfg.zoomMin() * 100);
    int sliderMax = qRound(cfg.zoomMax() * 100);
    int sliderStep = qRound(cfg.zoomStep() * 100);
    int sliderDefault = qRound(cfg.zoomDefault() * 100);

    m_fontSizeSlider = new QSlider(Qt::Horizontal);
    m_fontSizeSlider->setRange(sliderMin, sliderMax);
    m_fontSizeSlider->setSingleStep(sliderStep);
    m_fontSizeSlider->setPageStep(sliderStep);
    m_fontSizeSlider->setValue(sliderDefault);

    QString grooveColor = cfg.settingsPanelZoomSliderGrooveColor();
    int grooveHeight = cfg.settingsPanelZoomSliderGrooveHeight();
    int grooveRadius = cfg.settingsPanelZoomSliderGrooveRadius();
    QString handleColor = cfg.settingsPanelZoomSliderHandleColor();
    QString handleHoverColor = cfg.settingsPanelZoomSliderHandleHoverColor();
    int handleWidth = cfg.settingsPanelZoomSliderHandleWidth();
    int handleRadius = cfg.settingsPanelZoomSliderHandleRadius();

    m_fontSizeSlider->setStyleSheet(
        QStringLiteral(
            "QSlider::groove:horizontal {"
            "  background: %1;"
            "  height: %2px;"
            "  border-radius: %3px;"
            "}"
            "QSlider::handle:horizontal {"
            "  background: %4;"
            "  width: %5px;"
            "  margin: -5px 0;"
            "  border-radius: %6px;"
            "}"
            "QSlider::handle:horizontal:hover {"
            "  background: %7;"
            "}"
        ).arg(grooveColor)
         .arg(grooveHeight)
         .arg(grooveRadius)
         .arg(handleColor)
         .arg(handleWidth)
         .arg(handleRadius)
         .arg(handleHoverColor)
    );
    zoomRow->addWidget(m_fontSizeSlider, 1);

    m_fontSizeEdit = new QLineEdit;
    m_fontSizeEdit->setValidator(new ZoomValidator(0, 9999, m_fontSizeEdit));
    m_fontSizeEdit->setText(QString::number(sliderDefault));
    m_fontSizeEdit->setFixedWidth(cfg.settingsPanelZoomSpinboxWidth());
    m_fontSizeEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_fontSizeEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #3c3c3c;"
        "  color: #cccccc;"
        "  border: 1px solid #555555;"
        "  border-radius: 3px;"
        "  padding: 2px 4px;"
        "  font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #0078d4;"
        "}"
    );
    zoomRow->addWidget(m_fontSizeEdit);

    auto *percentLabel = new QLabel(QStringLiteral("%"));
    percentLabel->setStyleSheet("color: #cccccc; font-size: 12px;");
    zoomRow->addWidget(percentLabel);

    m_contentLayout->addLayout(zoomRow);
    m_contentLayout->addStretch();

    // 滑块 → 数字框
    connect(m_fontSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_fontSizeEdit->setText(QString::number(value));
    });

    // 数字框 → 滑块（编辑完成时，带边界钳位）
    connect(m_fontSizeEdit, &QLineEdit::editingFinished, this, [this, sliderMin, sliderMax]() {
        QString text = m_fontSizeEdit->text().trimmed();
        bool ok = false;
        int value = text.toInt(&ok);
        if (!ok || text.isEmpty()) {
            value = 100;
        }
        value = qBound(sliderMin, value, sliderMax);
        m_fontSizeEdit->setText(QString::number(value));
        m_fontSizeSlider->setValue(value);
    });

    // 默认缩放变更信号（仅从滑块发出，避免编辑中频繁触发）
    connect(m_fontSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        emit defaultZoomChanged(value / 100.0);
    });

    m_scrollArea->setWidget(scrollContent);

    // ---- Size grip ----
    m_sizeGrip = new QSizeGrip(this);
    m_sizeGrip->setStyleSheet("QSizeGrip { image: none; background: transparent; }");

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(m_scrollArea, 1);
}

QVBoxLayout *SettingsPanel::contentLayout() const
{
    return m_contentLayout;
}

QLabel *SettingsPanel::titleLabel() const
{
    return m_titleLabel;
}

void SettingsPanel::setDefaultZoom(qreal zoom)
{
    if (!m_fontSizeSlider) return;
    const auto &cfg = ConfigManager::instance();
    int sliderMin = qRound(cfg.zoomMin() * 100);
    int sliderMax = qRound(cfg.zoomMax() * 100);
    int value = qRound(zoom * 100);
    m_fontSizeSlider->setValue(qBound(sliderMin, value, sliderMax));
}

qreal SettingsPanel::defaultZoom() const
{
    if (!m_fontSizeSlider) return 1.0;
    return m_fontSizeSlider->value() / 100.0;
}

SettingsPanel::Edge SettingsPanel::detectEdge(const QPoint &pos) const
{
    const int x = pos.x();
    const int y = pos.y();
    const int w = width();
    const int h = height();

    bool onLeft   = (x >= 0 && x <= kEdgeMargin);
    bool onRight  = (x >= w - kEdgeMargin && x <= w);
    bool onTop    = (y >= 0 && y <= kEdgeMargin);
    bool onBottom = (y >= h - kEdgeMargin && y <= h);

    if (onTop    && onLeft)   return Edge::TopLeft;
    if (onTop    && onRight)  return Edge::TopRight;
    if (onBottom && onLeft)   return Edge::BottomLeft;
    if (onBottom && onRight)  return Edge::BottomRight;
    if (onTop)                return Edge::Top;
    if (onBottom)             return Edge::Bottom;
    if (onLeft)               return Edge::Left;
    if (onRight)              return Edge::Right;

    return Edge::None;
}

void SettingsPanel::updateCursorForEdge(Edge edge)
{
    switch (edge) {
    case Edge::Top:
    case Edge::Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Edge::Left:
    case Edge::Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case Edge::TopLeft:
    case Edge::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case Edge::TopRight:
    case Edge::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Edge::None:
    default:
        setCursor(Qt::ArrowCursor);
        break;
    }
}

void SettingsPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        Edge edge = detectEdge(event->pos());

        if (edge != Edge::None) {
            m_dragging = true;
            m_dragEdge = edge;
            m_dragStartPos = event->globalPos();
            m_dragStartGeometry = geometry();
        } else if (event->pos().y() < kTitleBarHeight) {
            m_dragging = true;
            m_dragEdge = Edge::None;
            m_dragStartPos = event->globalPos() - frameGeometry().topLeft();
        }
    }
    QWidget::mousePressEvent(event);
}

void SettingsPanel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPoint delta = event->globalPos() - m_dragStartPos;
        QWidget *overlay = parentWidget();

        if (m_dragEdge != Edge::None) {
            QRect newGeo = m_dragStartGeometry;
            switch (m_dragEdge) {
            case Edge::Right:       newGeo.setRight(m_dragStartGeometry.right() + delta.x()); break;
            case Edge::Left:        newGeo.setLeft(m_dragStartGeometry.left() + delta.x()); break;
            case Edge::Bottom:      newGeo.setBottom(m_dragStartGeometry.bottom() + delta.y()); break;
            case Edge::Top:         newGeo.setTop(m_dragStartGeometry.top() + delta.y()); break;
            case Edge::TopLeft:     newGeo.setTopLeft(m_dragStartGeometry.topLeft() + delta); break;
            case Edge::TopRight:    newGeo.setTopRight(m_dragStartGeometry.topRight() + QPoint(delta.x(), delta.y())); break;
            case Edge::BottomLeft:  newGeo.setBottomLeft(m_dragStartGeometry.bottomLeft() + QPoint(delta.x(), delta.y())); break;
            case Edge::BottomRight: newGeo.setBottomRight(m_dragStartGeometry.bottomRight() + delta); break;
            default: break;
            }

            if (newGeo.width() < m_minWidth) {
                if (m_dragEdge == Edge::Left || m_dragEdge == Edge::TopLeft || m_dragEdge == Edge::BottomLeft)
                    newGeo.setLeft(newGeo.right() - m_minWidth + 1);
                else
                    newGeo.setRight(newGeo.left() + m_minWidth - 1);
            }
            if (newGeo.height() < m_minHeight) {
                if (m_dragEdge == Edge::Top || m_dragEdge == Edge::TopLeft || m_dragEdge == Edge::TopRight)
                    newGeo.setTop(newGeo.bottom() - m_minHeight + 1);
                else
                    newGeo.setBottom(newGeo.top() + m_minHeight - 1);
            }

            if (overlay) {
                if (newGeo.left() < 0) newGeo.moveLeft(0);
                if (newGeo.top() < 0) newGeo.moveTop(0);
                if (newGeo.right() > overlay->width()) newGeo.moveRight(overlay->width());
                if (newGeo.bottom() > overlay->height()) newGeo.moveBottom(overlay->height());
            }

            setGeometry(newGeo);
        } else {
            QPoint newPos = event->globalPos() - m_dragStartPos;
            if (overlay) {
                newPos.setX(qBound(0, newPos.x(), overlay->width() - width()));
                newPos.setY(qBound(0, newPos.y(), overlay->height() - height()));
            }
            move(newPos);
        }
    } else {
        Edge edge = detectEdge(event->pos());
        updateCursorForEdge(edge);
    }
    QWidget::mouseMoveEvent(event);
}

void SettingsPanel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_dragEdge = Edge::None;
    }
    QWidget::mouseReleaseEvent(event);
}

bool SettingsPanel::event(QEvent *event)
{
    if (event->type() == QEvent::Leave) {
        if (!m_dragging) {
            setCursor(Qt::ArrowCursor);
        }
    }
    return QWidget::event(event);
}
