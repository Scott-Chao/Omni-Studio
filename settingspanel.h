#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QWidget>
#include <QVariant>
#include <QPainter>
#include <QMap>
#include <QVector>
#include <functional>

class QLabel;
class QListWidget;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QStackedWidget;
class QLineEdit;
class QVBoxLayout;
class QSizeGrip;
class QTextEdit;
class QTimer;
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
    void toolSettingChanged(const QString &key, const QVariant &value);
    void shortcutChanged(const QString &actionKey, const QString &keySequenceText);
    void resetToDefaultsRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

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
    QWidget *createShortcutsPage();
    QWidget *createAiServicePage();
    QWidget *createToolsPage();

    void refreshStyle();
    void refreshPageTree(QWidget *w);

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
    QSpinBox *m_markdownIndentWidthSpin = nullptr;
    ToggleSwitch *m_autoSaveToggle = nullptr;
    QSpinBox *m_autoSaveIntervalSpin = nullptr;

    // Output panel controls (moved to editor page)
    QComboBox *m_outputFontFamilyCombo = nullptr;
    QSpinBox *m_outputFontSizeSpin = nullptr;
    QSpinBox *m_outputMaxBlocksSpin = nullptr;

    // Preview controls
    QSpinBox *m_previewDebounceSpin = nullptr;
    QSpinBox *m_previewRatioSpin = nullptr;

    // Search controls
    QSpinBox *m_searchPerFileSpin = nullptr;
    QSpinBox *m_searchTotalSpin = nullptr;
    QSpinBox *m_searchSnippetSpin = nullptr;

    // Appearance page controls
    QComboBox *m_themeCombo = nullptr;
    QSpinBox *m_fileTreeItemHeightSpin = nullptr;
    QPushButton *m_resetThemeBtn = nullptr;
    ToggleSwitch *m_equalWidthTabToggle = nullptr;

    struct ColorControl {
        QPushButton *btn;
        QLabel *preview;
        QString configKey;
        QColor defaultColor;
    };
    QVector<ColorControl> m_colorControls;

    // Shortcuts
    QMap<QString, KeyRecorder*> m_keyRecorders;
    QWidget *m_shortcutsHeaderRow = nullptr;
    QWidget *m_shortcutsListContainer = nullptr;
    QPushButton *m_shortcutsResetBtn = nullptr;

    // AI service controls
    QComboBox *m_aiProviderCombo = nullptr;
    QLineEdit *m_aiEndpointEdit = nullptr;
    QLineEdit *m_aiApiKeyEdit = nullptr;
    QPushButton *m_aiApiKeyToggleBtn = nullptr;
    QLineEdit *m_aiModelEdit = nullptr;
    QSpinBox *m_aiMaxTokensSpin = nullptr;
    QTextEdit *m_aiSystemPromptEdit = nullptr;
    QTimer *m_aiPromptDebounceTimer = nullptr;

    // Tools page - Language Services
    QLineEdit *m_clangdPathEdit = nullptr;
    QPushButton *m_clangdBrowseBtn = nullptr;
    QLineEdit *m_clangdArgsEdit = nullptr;
    QLineEdit *m_pythonPathEdit = nullptr;
    QPushButton *m_pythonBrowseBtn = nullptr;

    // Tools page - Compiler
    QLineEdit *m_gxxFlagsEdit = nullptr;
    QLineEdit *m_msvcFlagsEdit = nullptr;

    // Tools page - Judge
    QSpinBox *m_judgeTimeLimitSpin = nullptr;
    QSpinBox *m_judgeMemoryLimitSpin = nullptr;

    // Tools page - OpenJudge
    QLineEdit *m_openJudgeUrlEdit = nullptr;
    ToggleSwitch *m_openJudgeAutoLoginToggle = nullptr;
    QLineEdit *m_openJudgeUsernameEdit = nullptr;
    QLineEdit *m_openJudgePasswordEdit = nullptr;
    QPushButton *m_openJudgePasswordToggleBtn = nullptr;

    bool m_dragging = false;
    Edge m_dragEdge = Edge::None;
    QPoint m_dragStartPos;
    QRect m_dragStartGeometry;

    int m_minWidth = 400;
    int m_minHeight = 300;
};

#endif // SETTINGSPANEL_H
