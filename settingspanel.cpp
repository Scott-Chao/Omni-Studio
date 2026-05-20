#include "settingspanel.h"
#include "configmanager.h"
#include "settingsmanager.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QSlider>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QMouseEvent>
#include <QApplication>
#include <QSizeGrip>
#include <QIntValidator>
#include <QTableWidget>
#include <QTextEdit>
#include <QHeaderView>
#include <QColorDialog>
#include <QFontDatabase>
#include <QPainter>
#include <QMessageBox>
#include <QMap>
#include <functional>
#include "keyrecorder.h"

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

// Shared style strings
const char *kLabelStyle = "color: #cccccc; font-size: 12px;";
const char *kInputStyle = "QSpinBox, QLineEdit, QComboBox {"
    "  background-color: #3c3c3c;"
    "  color: #cccccc;"
    "  border: 1px solid #555555;"
    "  border-radius: 3px;"
    "  padding: 2px 4px;"
    "  font-size: 12px;"
    "  min-height: 20px;"
    "}"
    "QSpinBox:focus, QLineEdit:focus, QComboBox:focus {"
    "  border-color: #0078d4;"
    "}"
    "QComboBox::drop-down {"
    "  border: none;"
    "  width: 20px;"
    "}"
    "QComboBox::down-arrow {"
    "  image: url(:/preview/spin-down.svg);"
    "  width: 10px;"
    "  height: 7px;"
    "  margin-right: 4px;"
    "}"
    "QComboBox QAbstractItemView {"
    "  background-color: #3c3c3c;"
    "  color: #cccccc;"
    "  border: 1px solid #555555;"
    "  selection-background-color: #0078d4;"
    "}"
    "QSpinBox::up-button, QSpinBox::down-button {"
    "  width: 20px;"
    "  border: none;"
    "  background-color: #3c3c3c;"
    "}"
    "QSpinBox::up-button {"
    "  subcontrol-origin: border;"
    "  subcontrol-position: top right;"
    "  border-left: 1px solid #555555;"
    "  border-top-right-radius: 3px;"
    "}"
    "QSpinBox::down-button {"
    "  subcontrol-origin: border;"
    "  subcontrol-position: bottom right;"
    "  border-left: 1px solid #555555;"
    "  border-bottom-right-radius: 3px;"
    "}"
    "QSpinBox::up-arrow {"
    "  image: url(:/preview/spin-up.svg);"
    "  width: 10px;"
    "  height: 7px;"
    "}"
    "QSpinBox::down-arrow {"
    "  image: url(:/preview/spin-down.svg);"
    "  width: 10px;"
    "  height: 7px;"
    "}";

} // namespace

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    const auto &cfg = ConfigManager::instance();
    int panelWidth = 680;
    int panelHeight = 480;
    m_minWidth = cfg.settingsPanelMinWidth();
    m_minHeight = cfg.settingsPanelMinHeight();

    resize(panelWidth, panelHeight);
    setObjectName("settingsPanel");

    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x2b, 0x2b, 0x2b));
    setPalette(pal);

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

    // ---- Body: categories | pages (no gap) ----
    auto *bodyWidget = new QWidget;
    bodyWidget->setStyleSheet("background: #2b2b2b;");
    auto *bodyLayout = new QHBoxLayout(bodyWidget);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // Left: category list + reset button
    auto *leftWidget = new QWidget;
    leftWidget->setFixedWidth(kCategoryWidth);
    leftWidget->setStyleSheet("background: #252525;");
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    m_categoryList = new QListWidget;
    m_categoryList->setFrameShape(QFrame::NoFrame);
    m_categoryList->setStyleSheet(
        "QListWidget {"
        "  background-color: #252525;"
        "  border: none;"
        "  border-right: 1px solid #3c3c3c;"
        "  color: #999999;"
        "  font-size: 13px;"
        "  padding: 4px 0;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  padding: 8px 16px;"
        "  border: none;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #2a2a2a;"
        "  color: #cccccc;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #2d2d2d;"
        "  color: #ffffff;"
        "  border-left: 3px solid #0078d4;"
        "  padding-left: 13px;"
        "}"
        "QScrollBar:vertical {"
        "  background-color: #252525;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    m_categoryList->addItem(tr("编辑器"));
    m_categoryList->addItem(tr("外观"));
    m_categoryList->addItem(tr("输出面板"));
    m_categoryList->addItem(tr("预览"));
    m_categoryList->addItem(tr("搜索"));
    m_categoryList->addItem(tr("快捷键"));
    m_categoryList->addItem(tr("AI 服务"));

    leftLayout->addWidget(m_categoryList, 1);

    // Reset to defaults button at bottom of sidebar
    auto *resetBtn = new QPushButton(tr("恢复默认设置"));
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-top: 1px solid #3c3c3c;"
        "  color: #999999;"
        "  font-size: 12px;"
        "  padding: 10px 16px;"
        "  text-align: left;"
        "}"
        "QPushButton:hover {"
        "  color: #ffffff;"
        "  background-color: #c42b1c;"
        "}"
    );
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("恢复默认设置"),
            tr("确定要恢复所有设置到默认值吗？此操作无法撤销。"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (reply == QMessageBox::Yes)
            emit resetToDefaultsRequested();
    });
    leftLayout->addWidget(resetBtn);

    // Right: stacked pages
    m_stackedWidget = new QStackedWidget;
    m_stackedWidget->setStyleSheet("background: #2b2b2b;");

    m_stackedWidget->addWidget(createEditorPage());       // index 0
    m_stackedWidget->addWidget(createAppearancePage());    // index 1
    m_stackedWidget->addWidget(createOutputPanelPage());   // index 2
    m_stackedWidget->addWidget(createPreviewPage());       // index 3
    m_stackedWidget->addWidget(createSearchPage());        // index 4
    m_stackedWidget->addWidget(createShortcutsPage());     // index 5
    m_stackedWidget->addWidget(createAiServicePage());     // index 6

    connect(m_categoryList, &QListWidget::currentRowChanged, m_stackedWidget, &QStackedWidget::setCurrentIndex);
    m_categoryList->setCurrentRow(0);

    bodyLayout->addWidget(leftWidget);
    bodyLayout->addWidget(m_stackedWidget, 1);

    // ---- Size grip ----
    m_sizeGrip = new QSizeGrip(this);
    m_sizeGrip->setStyleSheet("QSizeGrip { image: none; background: transparent; }");

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(bodyWidget, 1);
}

QLabel *SettingsPanel::createSectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: bold; margin-top: 4px; margin-bottom: 8px;");
    return label;
}

// ============================================================
// Page: 编辑器 (Editor)
// ============================================================
QWidget *SettingsPanel::createEditorPage()
{
    const auto &cfg = ConfigManager::instance();
    auto *page = new QWidget;
    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: #2b2b2b; border: none; }"
        "QScrollArea > QWidget > QWidget { background: #2b2b2b; }"
        "QScrollBar:vertical {"
        "  background-color: #2b2b2b;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    auto *content = new QWidget;
    content->setStyleSheet("background: #2b2b2b;");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    layout->addWidget(createSectionLabel(tr("编辑器")));

    // ---- 自动保存 ----
    auto *autoSaveRow = new QHBoxLayout;
    auto *autoSaveLabel = new QLabel(tr("自动保存"));
    autoSaveLabel->setStyleSheet(kLabelStyle);

    m_autoSaveToggle = new ToggleSwitch;
    m_autoSaveToggle->setChecked(true);

    auto *autoSaveStateLabel = new QLabel(tr("开"));
    autoSaveStateLabel->setStyleSheet("color: #2196F3; font-size: 13px;");

    m_autoSaveToggle->onToggled = [this, autoSaveStateLabel](bool checked) {
        autoSaveStateLabel->setText(checked ? tr("开") : tr("关"));
        autoSaveStateLabel->setStyleSheet(QString("color: %1; font-size: 13px;")
            .arg(checked ? "#2196F3" : "#999999"));
        emit editorSettingChanged("editor.auto_save", checked);
    };

    auto *toggleWidget = new QWidget;
    auto *toggleLayout = new QHBoxLayout(toggleWidget);
    toggleLayout->setContentsMargins(0, 0, 0, 0);
    toggleLayout->setSpacing(6);
    toggleLayout->addWidget(m_autoSaveToggle);
    toggleLayout->addWidget(autoSaveStateLabel);

    autoSaveRow->addWidget(autoSaveLabel);
    autoSaveRow->addStretch();
    autoSaveRow->addWidget(toggleWidget);
    layout->addLayout(autoSaveRow);

    layout->addSpacing(6);

    // ---- 默认字体大小 (zoom) ----
    auto *zoomRow = new QHBoxLayout;
    auto *zoomLabel = new QLabel(tr("默认缩放比例"));
    zoomLabel->setStyleSheet(kLabelStyle);
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
    m_fontSizeEdit->setStyleSheet(kInputStyle);
    zoomRow->addWidget(m_fontSizeEdit);

    auto *percentLabel = new QLabel(QStringLiteral("%"));
    percentLabel->setStyleSheet(kLabelStyle);
    zoomRow->addWidget(percentLabel);

    layout->addLayout(zoomRow);

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
    // 默认缩放变更信号
    connect(m_fontSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        emit defaultZoomChanged(value / 100.0);
    });

    // ---- 缩进宽度 ----
    int indentDef = cfg.editorIndentWidth();
    auto *indentRow = new QHBoxLayout;
    auto *indentLabel = new QLabel(tr("缩进宽度"));
    indentLabel->setStyleSheet(kLabelStyle);
    m_indentWidthSpin = new QSpinBox;
    m_indentWidthSpin->setRange(1, 8);
    m_indentWidthSpin->setValue(indentDef);
    m_indentWidthSpin->setFixedWidth(80);
    m_indentWidthSpin->setStyleSheet(kInputStyle);
    indentRow->addWidget(indentLabel);
    indentRow->addStretch();
    indentRow->addWidget(m_indentWidthSpin);
    layout->addLayout(indentRow);

    connect(m_indentWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit editorSettingChanged("editor.indent_width", val);
    });

    // ---- 编辑器字体 ----
    auto *fontRow = new QHBoxLayout;
    auto *fontLabel = new QLabel(tr("编辑器字体"));
    fontLabel->setStyleSheet(kLabelStyle);
    m_fontFamilyCombo = new QComboBox;
    m_fontFamilyCombo->setEditable(false);
    m_fontFamilyCombo->setFixedWidth(180);
    m_fontFamilyCombo->setStyleSheet(kInputStyle);

    static const QStringList families = QFontDatabase::families();
    m_fontFamilyCombo->addItems(families);
    int fontIdx = m_fontFamilyCombo->findText(cfg.editorFontFamily());
    if (fontIdx >= 0) m_fontFamilyCombo->setCurrentIndex(fontIdx);

    fontRow->addWidget(fontLabel);
    fontRow->addStretch();
    fontRow->addWidget(m_fontFamilyCombo);
    layout->addLayout(fontRow);

    connect(m_fontFamilyCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        emit editorSettingChanged("editor.font.family", text);
    });

    // ---- 编辑器字号 ----
    auto *fontSizeRow = new QHBoxLayout;
    auto *fontSizeLabel = new QLabel(tr("编辑器字号"));
    fontSizeLabel->setStyleSheet(kLabelStyle);
    m_fontSizeSpin = new QSpinBox;
    m_fontSizeSpin->setRange(8, 24);
    m_fontSizeSpin->setValue(cfg.editorFontSize());
    m_fontSizeSpin->setFixedWidth(80);
    m_fontSizeSpin->setStyleSheet(kInputStyle);
    fontSizeRow->addWidget(fontSizeLabel);
    fontSizeRow->addStretch();
    fontSizeRow->addWidget(m_fontSizeSpin);
    layout->addLayout(fontSizeRow);

    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit editorSettingChanged("editor.font.size", val);
    });

    layout->addStretch();

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    return page;
}

// ============================================================
// Page: 外观 (Appearance)
// ============================================================
QWidget *SettingsPanel::createAppearancePage()
{
    const auto &cfg = ConfigManager::instance();
    auto &sm = SettingsManager::instance();
    auto *page = new QWidget;
    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: #2b2b2b; border: none; }"
        "QScrollArea > QWidget > QWidget { background: #2b2b2b; }"
        "QScrollBar:vertical {"
        "  background-color: #2b2b2b;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    auto *content = new QWidget;
    content->setStyleSheet("background: #2b2b2b;");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    layout->addWidget(createSectionLabel(tr("编辑器颜色")));

    struct ColorItem {
        QString label;
        QString configKey;
        QColor defaultColor;
    };

    ColorItem colors[] = {
        {tr("编辑器背景"), "appearance.colors.editor.background", cfg.editorBackground()},
        {tr("编辑器前景（文字）"), "appearance.colors.editor.foreground", cfg.editorForeground()},
        {tr("选中背景"),  "appearance.colors.editor.selection",  cfg.editorSelection()},
        {tr("当前行高亮"), "appearance.colors.current_line.highlight", cfg.currentLineHighlight()},
        {tr("行号区域背景"), "appearance.colors.line_number.background", cfg.lineNumberBackground()},
        {tr("行号文字颜色"), "appearance.colors.line_number.foreground", cfg.lineNumberForeground()},
    };

    for (const auto &ci : colors) {
        auto *row = new QHBoxLayout;
        auto *lbl = new QLabel(ci.label);
        lbl->setStyleSheet(kLabelStyle);

        auto *colorBtn = new QPushButton;
        colorBtn->setFixedSize(24, 24);
        QString bgHex = sm.value(ci.configKey, ci.defaultColor.name()).toString();
        colorBtn->setStyleSheet(
            QStringLiteral(
                "QPushButton { background-color: %1; border: 1px solid #555555; border-radius: 4px; }"
                "QPushButton:hover { border-color: #0078d4; }"
            ).arg(bgHex)
        );

        auto *colorPreview = new QLabel;
        colorPreview->setFixedWidth(80);
        colorPreview->setText(bgHex);
        colorPreview->setStyleSheet(
            QStringLiteral("color: #999999; font-size: 11px; padding: 2px 4px; "
                           "background-color: #3c3c3c; border: 1px solid #555555; border-radius: 3px;")
        );

        QString configKey = ci.configKey;
        QColor defaultColor = ci.defaultColor;

        connect(colorBtn, &QPushButton::clicked, this, [this, colorBtn, colorPreview, configKey, defaultColor]() {
            QColor chosen = QColorDialog::getColor(defaultColor, this, tr("选择颜色"));
            if (chosen.isValid()) {
                QString hex = chosen.name();
                colorBtn->setStyleSheet(
                    QStringLiteral(
                        "QPushButton { background-color: %1; border: 1px solid #555555; border-radius: 4px; }"
                        "QPushButton:hover { border-color: #0078d4; }"
                    ).arg(hex)
                );
                colorPreview->setText(hex);
                emit appearanceSettingChanged(configKey, hex);
            }
        });

        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(colorPreview);
        row->addWidget(colorBtn);
        layout->addLayout(row);
    }

    layout->addStretch();
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    return page;
}

// ============================================================
// Page: 输出面板 (Output Panel)
// ============================================================
QWidget *SettingsPanel::createOutputPanelPage()
{
    const auto &cfg = ConfigManager::instance();
    auto *page = new QWidget;
    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: #2b2b2b; border: none; }"
        "QScrollArea > QWidget > QWidget { background: #2b2b2b; }"
        "QScrollBar:vertical {"
        "  background-color: #2b2b2b;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    auto *content = new QWidget;
    content->setStyleSheet("background: #2b2b2b;");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    layout->addWidget(createSectionLabel(tr("输出面板")));

    // 字体大小
    auto *sizeRow = new QHBoxLayout;
    auto *sizeLabel = new QLabel(tr("字体大小"));
    sizeLabel->setStyleSheet(kLabelStyle);
    m_outputFontSizeSpin = new QSpinBox;
    m_outputFontSizeSpin->setRange(8, 24);
    m_outputFontSizeSpin->setValue(cfg.outputPanelFontSize());
    m_outputFontSizeSpin->setFixedWidth(80);
    m_outputFontSizeSpin->setStyleSheet(kInputStyle);
    sizeRow->addWidget(sizeLabel);
    sizeRow->addStretch();
    sizeRow->addWidget(m_outputFontSizeSpin);
    layout->addLayout(sizeRow);

    connect(m_outputFontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit outputPanelSettingChanged("output_panel.font.size", val);
    });

    // 最大行数
    auto *maxRow = new QHBoxLayout;
    auto *maxLabel = new QLabel(tr("最大行数"));
    maxLabel->setStyleSheet(kLabelStyle);
    auto *maxSpin = new QSpinBox;
    maxSpin->setRange(100, 100000);
    maxSpin->setSingleStep(500);
    maxSpin->setValue(cfg.outputPanelMaxBlocks());
    maxSpin->setFixedWidth(100);
    maxSpin->setStyleSheet(kInputStyle);
    maxRow->addWidget(maxLabel);
    maxRow->addStretch();
    maxRow->addWidget(maxSpin);
    layout->addLayout(maxRow);

    connect(maxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit outputPanelSettingChanged("output_panel.max_blocks", val);
    });

    layout->addStretch();
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    return page;
}

// ============================================================
// Page: 预览 (Preview)
// ============================================================
QWidget *SettingsPanel::createPreviewPage()
{
    const auto &cfg = ConfigManager::instance();
    auto *page = new QWidget;
    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: #2b2b2b; border: none; }"
        "QScrollArea > QWidget > QWidget { background: #2b2b2b; }"
        "QScrollBar:vertical {"
        "  background-color: #2b2b2b;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    auto *content = new QWidget;
    content->setStyleSheet("background: #2b2b2b;");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    layout->addWidget(createSectionLabel(tr("预览")));

    // 分屏防抖毫秒
    auto *debounceRow = new QHBoxLayout;
    auto *debounceLabel = new QLabel(tr("分屏防抖 (ms)"));
    debounceLabel->setStyleSheet(kLabelStyle);
    m_previewDebounceSpin = new QSpinBox;
    m_previewDebounceSpin->setRange(100, 2000);
    m_previewDebounceSpin->setSingleStep(50);
    m_previewDebounceSpin->setValue(cfg.previewSplitDebounceMs());
    m_previewDebounceSpin->setSuffix(QStringLiteral(" ms"));
    m_previewDebounceSpin->setFixedWidth(100);
    m_previewDebounceSpin->setStyleSheet(kInputStyle);
    debounceRow->addWidget(debounceLabel);
    debounceRow->addStretch();
    debounceRow->addWidget(m_previewDebounceSpin);
    layout->addLayout(debounceRow);

    connect(m_previewDebounceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit previewSettingChanged("preview.split_debounce_ms", val);
    });

    // 分屏比例
    auto *ratioRow = new QHBoxLayout;
    auto *ratioLabel = new QLabel(tr("分屏比例"));
    ratioLabel->setStyleSheet(kLabelStyle);
    m_previewRatioSpin = new QSpinBox;
    m_previewRatioSpin->setRange(30, 70);
    m_previewRatioSpin->setValue(cfg.previewSplitPreviewRatio());
    m_previewRatioSpin->setFixedWidth(100);
    m_previewRatioSpin->setStyleSheet(kInputStyle);
    ratioRow->addWidget(ratioLabel);
    ratioRow->addStretch();
    ratioRow->addWidget(m_previewRatioSpin);
    auto *ratioPercentLabel = new QLabel(QStringLiteral("%"));
    ratioPercentLabel->setStyleSheet(kLabelStyle);
    ratioRow->addWidget(ratioPercentLabel);
    layout->addLayout(ratioRow);

    connect(m_previewRatioSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit previewSettingChanged("preview.split_preview_ratio", val);
    });

    layout->addStretch();
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    return page;
}

// ============================================================
// Page: 搜索 (Search)
// ============================================================
QWidget *SettingsPanel::createSearchPage()
{
    const auto &cfg = ConfigManager::instance();
    auto *page = new QWidget;
    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: #2b2b2b; border: none; }"
        "QScrollArea > QWidget > QWidget { background: #2b2b2b; }"
        "QScrollBar:vertical {"
        "  background-color: #2b2b2b;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    auto *content = new QWidget;
    content->setStyleSheet("background: #2b2b2b;");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    layout->addWidget(createSectionLabel(tr("搜索")));

    // 每文件最大匹配
    auto *perFileRow = new QHBoxLayout;
    auto *perFileLabel = new QLabel(tr("每文件最大匹配"));
    perFileLabel->setStyleSheet(kLabelStyle);
    m_searchPerFileSpin = new QSpinBox;
    m_searchPerFileSpin->setRange(1, 50);
    m_searchPerFileSpin->setValue(cfg.searchMaxPerFile());
    m_searchPerFileSpin->setFixedWidth(80);
    m_searchPerFileSpin->setStyleSheet(kInputStyle);
    perFileRow->addWidget(perFileLabel);
    perFileRow->addStretch();
    perFileRow->addWidget(m_searchPerFileSpin);
    layout->addLayout(perFileRow);

    connect(m_searchPerFileSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit searchSettingChanged("search_panel.max_per_file", val);
    });

    // 最大结果总数
    auto *totalRow = new QHBoxLayout;
    auto *totalLabel = new QLabel(tr("最大结果总数"));
    totalLabel->setStyleSheet(kLabelStyle);
    m_searchTotalSpin = new QSpinBox;
    m_searchTotalSpin->setRange(50, 2000);
    m_searchTotalSpin->setSingleStep(50);
    m_searchTotalSpin->setValue(cfg.searchMaxTotalResults());
    m_searchTotalSpin->setFixedWidth(100);
    m_searchTotalSpin->setStyleSheet(kInputStyle);
    totalRow->addWidget(totalLabel);
    totalRow->addStretch();
    totalRow->addWidget(m_searchTotalSpin);
    layout->addLayout(totalRow);

    connect(m_searchTotalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit searchSettingChanged("search_panel.max_total_results", val);
    });

    // 片段最大长度
    auto *snippetRow = new QHBoxLayout;
    auto *snippetLabel = new QLabel(tr("片段最大长度"));
    snippetLabel->setStyleSheet(kLabelStyle);
    m_searchSnippetSpin = new QSpinBox;
    m_searchSnippetSpin->setRange(50, 500);
    m_searchSnippetSpin->setSingleStep(10);
    m_searchSnippetSpin->setValue(cfg.searchSnippetMaxLength());
    m_searchSnippetSpin->setFixedWidth(80);
    m_searchSnippetSpin->setStyleSheet(kInputStyle);
    snippetRow->addWidget(snippetLabel);
    snippetRow->addStretch();
    snippetRow->addWidget(m_searchSnippetSpin);
    layout->addLayout(snippetRow);

    connect(m_searchSnippetSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit searchSettingChanged("search_panel.snippet_max_length", val);
    });

    layout->addStretch();
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    return page;
}

// ============================================================
// Page: 快捷键 (Shortcuts)
// ============================================================
QWidget *SettingsPanel::createShortcutsPage()
{
    const auto &cfg = ConfigManager::instance();
    auto *page = new QWidget;
    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: #2b2b2b; border: none; }"
        "QScrollArea > QWidget > QWidget { background: #2b2b2b; }"
        "QScrollBar:vertical {"
        "  background-color: #2b2b2b;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    auto *content = new QWidget;
    content->setStyleSheet("background: #2b2b2b;");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(2);

    layout->addWidget(createSectionLabel(tr("快捷键")));

    // 说明文字
    auto *descLabel = new QLabel(tr("点击快捷键可重新录制，按 Delete/Backspace 清除，按 Escape 取消"));
    descLabel->setStyleSheet("color: #999999; font-size: 11px; margin-bottom: 8px;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // Shortcut mapping: action name -> config key
    struct ShortcutItem {
        QString displayName;
        QString configKey;
    };
    ShortcutItem items[] = {
        {tr("新建文件"),           "new_file"},
        {tr("保存"),               "save"},
        {tr("另存为"),             "save_as"},
        {tr("切换预览"),           "toggle_preview"},
        {tr("分屏预览"),           "toggle_split_preview"},
        {tr("右侧面板"),           "toggle_right_panel"},
        {tr("历史记录"),           "toggle_history"},
        {tr("反向链接"),           "toggle_backlinks"},
        {tr("标签面板"),           "toggle_tags"},
        {tr("大纲面板"),           "toggle_outline"},
        {tr("全文搜索"),           "toggle_search"},
        {tr("AI 助手"),            "toggle_ai"},
        {tr("本地评测"),           "toggle_judge"},
        {tr("设置"),               "toggle_settings"},
        {tr("编译并运行"),         "compile_and_run"},
        {tr("仅编译"),             "compile_only"},
        {tr("仅运行"),             "run_only"},
        {tr("停止进程"),           "stop_process"},
        {tr("放大"),               "zoom_in"},
        {tr("缩小"),               "zoom_out"},
        {tr("重置缩放"),           "zoom_reset"},
        {tr("切换注释"),           "toggle_comment"},
        {tr("删除文件"),           "delete_file"},
        {tr("导出 PDF"),           "export_pdf"},
        {tr("转换 .md ↔ .smd"),   "convert_md_smd"},

        // --- CodeEditor ---
        {tr("代码补全"),           "completion_trigger"},
        {tr("向右缩进"),           "indent_right"},
        {tr("向左缩进"),           "indent_left"},

        // --- SmdEditor ---
        {tr("执行单元格"),         "cell_execute"},
        {tr("执行并跳转"),         "cell_execute_jump"},
        {tr("选择语言"),           "cell_language"},
        {tr("终止执行"),           "cell_terminate"},
        {tr("清除输出/取消渲染"),  "cell_clear_output"},
        {tr("分割单元格"),         "cell_split"},
        {tr("切换诊断面板"),       "toggle_diagnostics"},
        {tr("上方插入单元格"),     "cell_insert_above"},
        {tr("下方插入单元格"),     "cell_insert_below"},
        {tr("删除单元格"),         "cell_delete"},

        // --- OutputPanel ---
        {tr("输出面板-中断"),      "stop_in_output"},
        {tr("输出面板-粘贴"),      "paste_in_output"},
    };

    // Header row
    auto *headerRow = new QWidget;
    headerRow->setStyleSheet("background: #333333; border: 1px solid #3c3c3c; border-bottom: none;");
    auto *headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    auto *nameHeader = new QLabel(tr("操作"));
    nameHeader->setStyleSheet("color: #999999; font-weight: bold; font-size: 12px;");
    auto *keyHeader = new QLabel(tr("快捷键"));
    keyHeader->setStyleSheet("color: #999999; font-weight: bold; font-size: 12px;");
    headerLayout->addWidget(nameHeader, 1);
    headerLayout->addWidget(keyHeader, 1);
    layout->addWidget(headerRow);

    // Build a container for the list
    auto *listContainer = new QWidget;
    listContainer->setStyleSheet("background: #2b2b2b; border: 1px solid #3c3c3c;");
    auto *listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    // Conflict detection lambda
    auto checkConflict = [this](KeyRecorder *sender, const QString &newKs) {
        if (newKs.isEmpty())
            return QString();
        for (auto it = m_keyRecorders.constBegin(); it != m_keyRecorders.constEnd(); ++it) {
            if (it.value() == sender)
                continue;
            if (it.value()->keySequence() == newKs)
                return it.key();
        }
        return QString();
    };

    m_keyRecorders.clear();

    for (const auto &item : items) {
        auto *row = new QWidget;
        row->setFixedHeight(32);
        row->setStyleSheet("background: transparent;");

        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(8, 0, 8, 0);
        rowLayout->setSpacing(8);

        auto *label = new QLabel(item.displayName);
        label->setStyleSheet("color: #cccccc; font-size: 12px;");
        rowLayout->addWidget(label, 1);

        QString shortcutStr = cfg.shortcut(item.configKey, "");
        auto *recorder = new KeyRecorder(item.configKey, shortcutStr);
        rowLayout->addWidget(recorder, 1);

        m_keyRecorders[item.configKey] = recorder;

        connect(recorder, &KeyRecorder::keySequenceCaptured,
                this, [this, recorder, checkConflict, item](const QString &actionKey, const QKeySequence &ks) {
            QString newText = ks.toString(QKeySequence::NativeText);

            // Conflict detection
            QString conflictAction = checkConflict(recorder, newText);
            if (!conflictAction.isEmpty()) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle(tr("快捷键冲突"));
                msgBox.setText(tr("快捷键 %1 已被「%2」使用。\n是否将 %1 分配给「%3」？")
                               .arg(newText, conflictAction, item.configKey));
                msgBox.setIcon(QMessageBox::Warning);
                auto *overwriteBtn = msgBox.addButton(tr("覆盖"), QMessageBox::AcceptRole);
                msgBox.addButton(tr("取消"), QMessageBox::RejectRole);
                msgBox.exec();

                if (msgBox.clickedButton() == overwriteBtn) {
                    if (m_keyRecorders.contains(conflictAction)) {
                        m_keyRecorders[conflictAction]->setKeySequence(QString());
                        emit shortcutChanged(conflictAction, QString());
                    }
                    recorder->setKeySequence(newText);
                    emit shortcutChanged(actionKey, newText);
                } else {
                    recorder->restorePreviousSequence();
                }
                return;
            }

            // No conflict
            recorder->setKeySequence(newText);
            emit shortcutChanged(actionKey, newText);
        });

        listLayout->addWidget(row);

        // Separator
        auto *sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #3c3c3c;");
        sep->setFixedHeight(1);
        listLayout->addWidget(sep);
    }

    layout->addWidget(listContainer, 1);

    // Reset all button
    auto *resetBtn = new QPushButton(tr("恢复默认"));
    resetBtn->setStyleSheet(
        "QPushButton {"
        "  background: #3c3c3c; color: #cccccc; border: 1px solid #555;"
        "  padding: 6px 16px; border-radius: 3px; font-size: 12px;"
        "}"
        "QPushButton:hover { background: #4c4c4c; }"
    );
    resetBtn->setFixedWidth(120);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto &cfg2 = ConfigManager::instance();
        for (auto it = m_keyRecorders.begin(); it != m_keyRecorders.end(); ++it) {
            QString defaultVal = cfg2.shortcut(it.key(), "");
            it.value()->setKeySequence(defaultVal);
            emit shortcutChanged(it.key(), defaultVal);
        }
    });
    layout->addWidget(resetBtn, 0, Qt::AlignLeft);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    return page;
}

// ============================================================
// Page: AI 服务 (AI Service)
// ============================================================
QWidget *SettingsPanel::createAiServicePage()
{
    const auto &cfg = ConfigManager::instance();
    auto *page = new QWidget;
    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: #2b2b2b; border: none; }"
        "QScrollArea > QWidget > QWidget { background: #2b2b2b; }"
        "QScrollBar:vertical {"
        "  background-color: #2b2b2b;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #555555;"
        "  min-height: 30px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );

    auto *content = new QWidget;
    content->setStyleSheet("background: #2b2b2b;");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    layout->addWidget(createSectionLabel(tr("AI 服务")));

    // ---- API 类型 ----
    auto *providerRow = new QHBoxLayout;
    auto *providerLabel = new QLabel(tr("API 类型"));
    providerLabel->setStyleSheet(kLabelStyle);
    m_aiProviderCombo = new QComboBox;
    m_aiProviderCombo->addItems({QStringLiteral("Anthropic"), QStringLiteral("OpenAI")});
    m_aiProviderCombo->setFixedWidth(180);
    m_aiProviderCombo->setStyleSheet(kInputStyle);
    providerRow->addWidget(providerLabel);
    providerRow->addStretch();
    providerRow->addWidget(m_aiProviderCombo);
    layout->addLayout(providerRow);

    connect(m_aiProviderCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        emit aiSettingChanged("ai.provider_type", text);
    });

    // ---- API 端点 ----
    auto *endpointRow = new QHBoxLayout;
    auto *endpointLabel = new QLabel(tr("API 端点"));
    endpointLabel->setStyleSheet(kLabelStyle);
    m_aiEndpointEdit = new QLineEdit;
    m_aiEndpointEdit->setPlaceholderText(QStringLiteral("https://api.deepseek.com/v1"));
    m_aiEndpointEdit->setStyleSheet(kInputStyle);
    endpointRow->addWidget(endpointLabel);
    endpointRow->addStretch();
    endpointRow->addWidget(m_aiEndpointEdit, 1);
    layout->addLayout(endpointRow);

    connect(m_aiEndpointEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit aiSettingChanged("ai.endpoint", text);
    });

    // ---- API Key ----
    auto *keyRow = new QHBoxLayout;
    auto *keyLabel = new QLabel(tr("API Key"));
    keyLabel->setStyleSheet(kLabelStyle);
    m_aiApiKeyEdit = new QLineEdit;
    m_aiApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_aiApiKeyEdit->setPlaceholderText(tr("输入 API Key"));
    m_aiApiKeyEdit->setStyleSheet(kInputStyle);
    keyRow->addWidget(keyLabel);
    keyRow->addStretch();
    keyRow->addWidget(m_aiApiKeyEdit, 1);
    layout->addLayout(keyRow);

    connect(m_aiApiKeyEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit aiSettingChanged("ai.api_key", text);
    });

    // ---- 模型 ----
    auto *modelRow = new QHBoxLayout;
    auto *modelLabel = new QLabel(tr("模型"));
    modelLabel->setStyleSheet(kLabelStyle);
    m_aiModelEdit = new QLineEdit;
    m_aiModelEdit->setPlaceholderText(QStringLiteral("deepseek-v4-flash"));
    m_aiModelEdit->setStyleSheet(kInputStyle);
    modelRow->addWidget(modelLabel);
    modelRow->addStretch();
    modelRow->addWidget(m_aiModelEdit, 1);
    layout->addLayout(modelRow);

    connect(m_aiModelEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit aiSettingChanged("ai.model", text);
    });

    // ---- Max Tokens ----
    auto *tokensRow = new QHBoxLayout;
    auto *tokensLabel = new QLabel(tr("Max Tokens"));
    tokensLabel->setStyleSheet(kLabelStyle);
    m_aiMaxTokensSpin = new QSpinBox;
    m_aiMaxTokensSpin->setRange(256, 16384);
    m_aiMaxTokensSpin->setSingleStep(256);
    m_aiMaxTokensSpin->setValue(cfg.aiMaxTokens());
    m_aiMaxTokensSpin->setFixedWidth(120);
    m_aiMaxTokensSpin->setStyleSheet(kInputStyle);
    tokensRow->addWidget(tokensLabel);
    tokensRow->addStretch();
    tokensRow->addWidget(m_aiMaxTokensSpin);
    layout->addLayout(tokensRow);

    connect(m_aiMaxTokensSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit aiSettingChanged("ai.max_tokens", val);
    });

    // ---- 系统提示词 ----
    layout->addSpacing(8);
    auto *promptLabel = new QLabel(tr("系统提示词"));
    promptLabel->setStyleSheet(kLabelStyle);
    layout->addWidget(promptLabel);

    m_aiSystemPromptEdit = new QTextEdit;
    m_aiSystemPromptEdit->setPlaceholderText(tr("可选的系统提示词，设定 AI 助手的角色和行为..."));
    m_aiSystemPromptEdit->setFixedHeight(120);
    m_aiSystemPromptEdit->setStyleSheet(
        "QTextEdit {"
        "  background-color: #3c3c3c;"
        "  color: #cccccc;"
        "  border: 1px solid #555555;"
        "  border-radius: 3px;"
        "  padding: 4px;"
        "  font-size: 12px;"
        "}"
        "QTextEdit:focus {"
        "  border-color: #0078d4;"
        "}"
    );
    layout->addWidget(m_aiSystemPromptEdit);

    connect(m_aiSystemPromptEdit, &QTextEdit::textChanged, this, [this]() {
        emit aiSettingChanged("ai.system_prompt", m_aiSystemPromptEdit->toPlainText());
    });

    layout->addStretch();
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    return page;
}

// ============================================================
// Drag & Resize (unchanged)
// ============================================================

void SettingsPanel::syncFromSettings(SettingsManager &sm)
{
    const auto &cfg = ConfigManager::instance();

    // Editor page
    if (m_fontFamilyCombo) {
        QString family = sm.value("editor.font.family", "").toString();
        if (!family.isEmpty()) {
            int idx = m_fontFamilyCombo->findText(family);
            if (idx >= 0) m_fontFamilyCombo->setCurrentIndex(idx);
        }
    }
    if (m_fontSizeSpin) {
        m_fontSizeSpin->setValue(sm.value("editor.font.size", cfg.editorFontSize()).toInt());
    }
    if (m_indentWidthSpin) {
        m_indentWidthSpin->setValue(sm.value("editor.indent_width", cfg.editorIndentWidth()).toInt());
    }
    if (m_autoSaveToggle) {
        m_autoSaveToggle->setChecked(sm.value("editor.auto_save", cfg.autoSaveEnabled()).toBool());
    }

    // Output panel page
    if (m_outputFontSizeSpin) {
        m_outputFontSizeSpin->setValue(sm.value("output_panel.font.size", cfg.outputPanelFontSize()).toInt());
    }

    // Preview page
    if (m_previewDebounceSpin) {
        m_previewDebounceSpin->setValue(sm.value("preview.split_debounce_ms", cfg.previewSplitDebounceMs()).toInt());
    }
    if (m_previewRatioSpin) {
        m_previewRatioSpin->setValue(sm.value("preview.split_preview_ratio", cfg.previewSplitPreviewRatio()).toInt());
    }

    // Search page
    if (m_searchPerFileSpin) {
        m_searchPerFileSpin->setValue(sm.value("search_panel.max_per_file", cfg.searchMaxPerFile()).toInt());
    }
    if (m_searchTotalSpin) {
        m_searchTotalSpin->setValue(sm.value("search_panel.max_total_results", cfg.searchMaxTotalResults()).toInt());
    }
    if (m_searchSnippetSpin) {
        m_searchSnippetSpin->setValue(sm.value("search_panel.snippet_max_length", cfg.searchSnippetMaxLength()).toInt());
    }

    // AI service page
    if (m_aiProviderCombo) {
        QString provider = sm.value("ai.provider_type", cfg.aiProviderType()).toString();
        int idx = m_aiProviderCombo->findText(provider);
        if (idx >= 0) m_aiProviderCombo->setCurrentIndex(idx);
    }
    if (m_aiEndpointEdit)
        m_aiEndpointEdit->setText(sm.value("ai.endpoint", cfg.aiEndpoint()).toString());
    if (m_aiApiKeyEdit)
        m_aiApiKeyEdit->setText(sm.aiApiKey());
    if (m_aiModelEdit)
        m_aiModelEdit->setText(sm.value("ai.model", cfg.aiModel()).toString());
    if (m_aiMaxTokensSpin)
        m_aiMaxTokensSpin->setValue(sm.value("ai.max_tokens", cfg.aiMaxTokens()).toInt());
    if (m_aiSystemPromptEdit)
        m_aiSystemPromptEdit->setPlainText(sm.value("ai.system_prompt", cfg.aiSystemPrompt()).toString());

    // Shortcuts page: sync overrides to KeyRecorder widgets
    for (auto it = m_keyRecorders.begin(); it != m_keyRecorders.end(); ++it) {
        QString val = sm.value("shortcuts." + it.key(), "").toString();
        if (!val.isEmpty())
            it.value()->setKeySequence(val);
    }
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
            m_dragStartPos = event->globalPosition().toPoint();
            m_dragStartGeometry = geometry();
        } else if (event->pos().y() < kTitleBarHeight) {
            m_dragging = true;
            m_dragEdge = Edge::None;
            m_dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        }
    }
    QWidget::mousePressEvent(event);
}

void SettingsPanel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
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
            QPoint newPos = event->globalPosition().toPoint() - m_dragStartPos;
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
