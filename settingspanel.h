#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QWidget>
#include <QVariant>

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

    // Output panel controls
    QSpinBox *m_outputFontSizeSpin = nullptr;

    // Preview controls
    QSpinBox *m_previewDebounceSpin = nullptr;
    QSpinBox *m_previewRatioSpin = nullptr;

    // Search controls
    QSpinBox *m_searchPerFileSpin = nullptr;
    QSpinBox *m_searchTotalSpin = nullptr;
    QSpinBox *m_searchSnippetSpin = nullptr;

    bool m_dragging = false;
    Edge m_dragEdge = Edge::None;
    QPoint m_dragStartPos;
    QRect m_dragStartGeometry;

    int m_minWidth = 400;
    int m_minHeight = 300;
};

#endif // SETTINGSPANEL_H
