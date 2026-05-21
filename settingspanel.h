#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QWidget>
#include <QVariant>
#include <QPainter>
#include <QMap>
#include <functional>

class QLabel;
class QListWidget;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QComboBox;
class QStackedWidget;
class QLineEdit;
class QVBoxLayout;
class QSizeGrip;
class QTextEdit;
class KeyRecorder;

// 自定义开关控件，类似 Windows 系统设置的 Toggle Switch
class ToggleSwitch : public QWidget
{
public:
    explicit ToggleSwitch(QWidget *parent = nullptr)
        : QWidget(parent) { setFixedSize(44, 24); setCursor(Qt::PointingHandCursor); }

    bool isChecked() const { return m_checked; }
    void setChecked(bool checked) {
        if (m_checked != checked) { m_checked = checked; update(); if (onToggled) onToggled(checked); }
    }

    std::function<void(bool)> onToggled;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int w = width(), h = height();
        int trackH = 14, trackY = (h - trackH) / 2;
        int thumbSize = 20, thumbY = (h - thumbSize) / 2;
        p.setPen(Qt::NoPen);
        p.setBrush(m_checked ? QColor("#2196F3") : QColor("#999999"));
        p.drawRoundedRect(1, trackY, w - 2, trackH, trackH / 2, trackH / 2);
        int thumbX = m_checked ? w - thumbSize - 2 : 2;
        p.setBrush(Qt::white);
        p.setPen(QPen(QColor("#cccccc"), 1));
        p.drawEllipse(QPointF(thumbX + thumbSize / 2.0, thumbY + thumbSize / 2.0), thumbSize / 2.0, thumbSize / 2.0);
    }
    void mousePressEvent(QMouseEvent *) override { setChecked(!m_checked); }

private:
    bool m_checked = true;
};

class SettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    void setDefaultZoom(qreal zoom);
    qreal defaultZoom() const;

    // 从 SettingsManager 覆盖值同步控件初始值（面板打开时调用）
    void syncFromSettings(class SettingsManager &sm);

signals:
    void closeRequested();
    void defaultZoomChanged(qreal zoom);
    void editorSettingChanged(const QString &key, const QVariant &value);
    void appearanceSettingChanged(const QString &key, const QVariant &value);
    void outputPanelSettingChanged(const QString &key, const QVariant &value);
    void previewSettingChanged(const QString &key, const QVariant &value);
    void searchSettingChanged(const QString &key, const QVariant &value);
    void aiSettingChanged(const QString &key, const QVariant &value);
    void shortcutChanged(const QString &actionKey, const QString &keySequenceText);
    void resetToDefaultsRequested();

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

    static constexpr int kEdgeMargin = 0;
    static constexpr int kTitleBarHeight = 36;
    static constexpr int kCategoryWidth = 170;

    Edge detectEdge(const QPoint &pos) const;
    void updateCursorForEdge(Edge edge);

    // Category pages
    QWidget *createEditorPage();
    QWidget *createAppearancePage();
    QWidget *createOutputPanelPage();
    QWidget *createPreviewPage();
    QWidget *createSearchPage();
    QWidget *createShortcutsPage();
    QWidget *createAiServicePage();

    // Shared helper for consistent section label styling
    QLabel *createSectionLabel(const QString &text);

    QLabel *m_titleLabel;
    QPushButton *m_closeButton;
    QListWidget *m_categoryList;
    QStackedWidget *m_stackedWidget;
    QSizeGrip *m_sizeGrip;

    // Editor page controls
    QSlider *m_fontSizeSlider = nullptr;
    QLineEdit *m_fontSizeEdit = nullptr;
    QComboBox *m_fontFamilyCombo = nullptr;
    QSpinBox *m_fontSizeSpin = nullptr;
    QSpinBox *m_indentWidthSpin = nullptr;
    ToggleSwitch *m_autoSaveToggle = nullptr;

    // Output panel controls
    QSpinBox *m_outputFontSizeSpin = nullptr;

    // Preview controls
    QSpinBox *m_previewDebounceSpin = nullptr;
    QSpinBox *m_previewRatioSpin = nullptr;

    // Search controls
    QSpinBox *m_searchPerFileSpin = nullptr;
    QSpinBox *m_searchTotalSpin = nullptr;
    QSpinBox *m_searchSnippetSpin = nullptr;

    // Appearance page controls
    QComboBox *m_themeCombo = nullptr;

    // Shortcuts
    QMap<QString, KeyRecorder*> m_keyRecorders;

    // AI service controls
    QComboBox *m_aiProviderCombo = nullptr;
    QLineEdit *m_aiEndpointEdit = nullptr;
    QLineEdit *m_aiApiKeyEdit = nullptr;
    QLineEdit *m_aiModelEdit = nullptr;
    QSpinBox *m_aiMaxTokensSpin = nullptr;
    QTextEdit *m_aiSystemPromptEdit = nullptr;

    bool m_dragging = false;
    Edge m_dragEdge = Edge::None;
    QPoint m_dragStartPos;
    QRect m_dragStartGeometry;

    int m_minWidth = 400;
    int m_minHeight = 300;
};

#endif // SETTINGSPANEL_H
