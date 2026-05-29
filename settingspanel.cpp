#include "settingspanel.h"
#include "configmanager.h"
#include "settingsmanager.h"
#include "thememanager.h"

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
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QMouseEvent>
#include <QApplication>
#include <QSizeGrip>
#include <QIntValidator>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QHeaderView>
#include <QColorDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QPainter>
#include <QMessageBox>
#include <QMap>
#include <QTimer>
#include <QPointer>
#include <QScreen>
#include <QGuiApplication>
#include <functional>
#include "keyrecorder.h"

// ============================================================
// FontDropdown — QComboBox subclass that bypasses Qt's broken
// stylesheet-popup sizing by using a custom popup QListWidget.
// ============================================================
class FontDropdown : public QComboBox
{
    Q_OBJECT
public:
    using QComboBox::QComboBox;

    void showPopup() override
    {
        if (count() == 0)
            return;

        closePopup();

        auto &tm = ThemeManager::instance();

        m_popup = new QListWidget;
        m_popup->setWindowFlags(Qt::Popup);
        m_popup->setAttribute(Qt::WA_DeleteOnClose);
        m_popup->setFont(font());

        const int itemH = qMax(fontMetrics().height() + 8, 28);
        constexpr int kMaxVisible = 10;
        const int visible = qMin(count(), kMaxVisible);
        const int popupW = width();
        const int popupH = visible * itemH + 2;

        for (int i = 0; i < count(); ++i) {
            auto *item = new QListWidgetItem(itemText(i), m_popup);
            item->setSizeHint(QSize(0, itemH));
        }

        m_popup->setCurrentRow(currentIndex());
        m_popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_popup->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        QPoint pos = mapToGlobal(QPoint(0, height()));
        if (auto *screen = QGuiApplication::screenAt(pos)) {
            const QRect avail = screen->availableGeometry();
            if (pos.y() + popupH > avail.bottom())
                pos.setY(qMax(avail.top(), mapToGlobal(QPoint(0, 0)).y() - popupH));
        }

        m_popup->setGeometry(pos.x(), pos.y(), popupW, popupH);

        const QString bg   = tm.color("menu.background").name();
        const QString fg   = tm.color("input.foreground").name();
        const QString bd   = tm.color("input.border").name();
        const QString sel  = tm.color("badge.background").name();
        const QString hbar = tm.color("scrollbarSlider.hoverBackground").name();

        m_popup->setStyleSheet(QStringLiteral(
            "QListWidget { background-color: %1; color: %2; border: 1px solid %3; outline: none; }"
            "QListWidget::item { padding: 0px 8px; }"
            "QListWidget::item:selected { background-color: %4; }"
            "QScrollBar:vertical { background: %1; width: 10px; margin: 0; }"
            "QScrollBar::handle:vertical { background: %5; min-height: 30px; border-radius: 5px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        ).arg(bg, fg, bd, sel, hbar));

        connect(m_popup, &QListWidget::itemClicked, this, &FontDropdown::onItemSelected);
        connect(m_popup, &QListWidget::itemActivated, this, &FontDropdown::onItemSelected);

        qApp->installEventFilter(this);
        m_popup->show();
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!m_popup)
            return QComboBox::eventFilter(watched, event);

        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            QPoint gp = me->globalPosition().toPoint();
            QRect popupRect(m_popup->mapToGlobal(QPoint(0, 0)), m_popup->size());
            QRect comboRect(mapToGlobal(QPoint(0, 0)), size());
            if (!popupRect.contains(gp) && !comboRect.contains(gp)) {
                closePopup();
                // Don't consume — let the click reach its target
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Escape) {
                closePopup();
                return true; // consume Escape
            }
        }
        return QComboBox::eventFilter(watched, event);
    }

private:
    void closePopup()
    {
        if (m_popup) {
            qApp->removeEventFilter(this);
            m_popup->close();
            m_popup = nullptr;
        }
    }

    void onItemSelected(QListWidgetItem *item)
    {
        if (!m_popup)
            return;
        setCurrentIndex(m_popup->row(item));
        closePopup();
    }

    QPointer<QListWidget> m_popup;
};

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

// QSpinBox subclass: allows out-of-range input, auto-clamps on confirm
class ClampSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;
    QValidator::State validate(QString &input, int &pos) const override
    {
        Q_UNUSED(pos);
        bool ok;
        int val = input.toInt(&ok);
        if (!ok) return QValidator::Intermediate;
        if (val < minimum() || val > maximum())
            return QValidator::Intermediate;
        return QValidator::Acceptable;
    }
    void fixup(QString &input) const override
    {
        bool ok;
        int val = input.toInt(&ok);
        if (ok)
            input = QString::number(qBound(minimum(), val, maximum()));
    }
};

static QString labelStyle() {
    auto &tm = ThemeManager::instance();
    return QStringLiteral("color: %1; font-size: 12px;")
        .arg(tm.color("workbench.foreground").name());
}

// Helper: create a row label with explicit alignment & zero contents margins.
// Without explicit contentsMargins(0,0,0,0), QStyleSheetStyle inherited from
// the parent #settingsPanel may inject platform-varying default padding.
static QLabel *makeRowLabel(const QString &text) {
    auto *lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lbl->setContentsMargins(0, 0, 0, 0);
    lbl->setStyleSheet(labelStyle());
    return lbl;
}

static QString inputStyle() {
    auto &tm = ThemeManager::instance();
    QString bg = tm.color("input.background").name();
    QString fg = tm.color("input.foreground").name();
    QString border = tm.color("input.border").name();
    QString accent = tm.color("badge.background").name();
    QString hoverBg = tm.color("aiAssistant.actionButtonHoverBackground").name();
    return QStringLiteral(
        "QSpinBox, QLineEdit, QComboBox { background-color: %1; color: %2; border: 1px solid %3; border-radius: 3px; padding: 2px 4px; font-size: 12px; min-height: 20px; }"
        "QSpinBox:focus, QLineEdit:focus, QComboBox:focus { border-color: %4; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox::down-arrow { image: url(:/preview/spin-down.svg); width: 10px; height: 7px; }"
        "QComboBox QAbstractItemView { background-color: %1; color: %2; border: 1px solid %3; selection-background-color: %4; outline: none; max-height: 240px; padding: 0px; }"
        "QComboBox QAbstractItemView::item { min-height: 24px; padding: 2px 8px; }"
        "QSpinBox::up-button, QSpinBox::down-button { width: 0px; border: none; }"
        "QComboBox::drop-down:hover { background-color: %5; }"
    ).arg(bg, fg, border, accent, hoverBg);
}



static QString scrollAreaStyle() {
    auto &tm = ThemeManager::instance();
    QString bg = tm.color("menu.background").name();
    QString handle = tm.color("scrollbarSlider.hoverBackground").name();
    return QStringLiteral(
        "QScrollArea { background: %1; border: none; }"
        "QScrollArea > QWidget > QWidget { background: %1; }"
        "QScrollBar:vertical { background-color: %1; width: 10px; margin: 0; }"
        "QScrollBar::handle:vertical { background-color: %2; min-height: 30px; border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "").arg(bg, handle);
}

static QString sectionLabelStyle() {
    auto &tm = ThemeManager::instance();
    return QStringLiteral("color: %1; font-size: 14px; font-weight: bold; margin-top: 4px; margin-bottom: 8px;")
        .arg(tm.color("tab.activeForeground").name());
}

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
    pal.setColor(QPalette::Window, ThemeManager::instance().color("menu.background"));
    setPalette(pal);

    setMouseTracking(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- Title bar ----
    auto *titleBar = new QWidget;
    titleBar->setFixedHeight(kTitleBarHeight);
    titleBar->setObjectName("settingsTitleBar");

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 8, 0);

    m_titleLabel = new QLabel(tr("设置"));

    m_closeButton = new QPushButton(QStringLiteral("✕"));
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setFlat(true);
    m_closeButton->setCursor(Qt::ArrowCursor);
    connect(m_closeButton, &QPushButton::clicked, this, &SettingsPanel::closeRequested);

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_closeButton);

    // ---- Body: categories | pages (no gap) ----
    auto *bodyWidget = new QWidget;
    auto *bodyLayout = new QHBoxLayout(bodyWidget);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // Left: category list + reset button
    auto *leftWidget = new QWidget;
    leftWidget->setFixedWidth(kCategoryWidth);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    m_categoryList = new QListWidget;
    m_categoryList->setFrameShape(QFrame::NoFrame);

    m_categoryList->addItem(tr("编辑器"));
    m_categoryList->addItem(tr("外观"));
    m_categoryList->addItem(tr("AI 服务"));
    m_categoryList->addItem(tr("快捷键"));
    m_categoryList->addItem(tr("工具"));

    leftLayout->addWidget(m_categoryList, 1);

    // Reset to defaults button at bottom of sidebar
    auto *resetBtn = new QPushButton(tr("恢复默认设置"));
    resetBtn->setCursor(Qt::PointingHandCursor);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("恢复默认设置"),
            tr("确定要恢复所有设置到默认值吗？此操作无法撤销。"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (reply == QMessageBox::Yes) {
            emit resetToDefaultsRequested();
            syncFromSettings(SettingsManager::instance());
        }
    });
    leftLayout->addWidget(resetBtn);

    // Right: stacked pages
    m_stackedWidget = new QStackedWidget;

    m_stackedWidget->addWidget(createEditorPage());       // index 0
    m_stackedWidget->addWidget(createAppearancePage());    // index 1
    m_stackedWidget->addWidget(createAiServicePage());     // index 2
    m_stackedWidget->addWidget(createShortcutsPage());     // index 3
    m_stackedWidget->addWidget(createToolsPage());         // index 4

    connect(m_categoryList, &QListWidget::currentRowChanged, m_stackedWidget, &QStackedWidget::setCurrentIndex);
    m_categoryList->setCurrentRow(0);

    bodyLayout->addWidget(leftWidget);
    bodyLayout->addWidget(m_stackedWidget, 1);

    // ---- Size grip ----
    m_sizeGrip = new QSizeGrip(this);
    m_sizeGrip->setStyleSheet("QSizeGrip { image: none; background: transparent; }");

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(bodyWidget, 1);

    refreshStyle();
}

void SettingsPanel::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, tm.color("menu.background"));
    setPalette(pal);

    setStyleSheet(QStringLiteral(
        "#settingsPanel {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}")
        .arg(tm.color("menu.background").name(),
             tm.color("panel.border").name()));

    if (auto *titleBar = findChild<QWidget*>("settingsTitleBar")) {
        titleBar->setStyleSheet(QStringLiteral(
            "#settingsTitleBar {"
            "  background-color: %1;"
            "  border-top-left-radius: 8px;"
            "  border-top-right-radius: 8px;"
            "}")
            .arg(tm.color("activityBar.background").name()));
    }

    m_titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: bold;")
                                .arg(tm.color("titleBar.foreground").name()));

    m_closeButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: %1; border: none; font-size: 14px; }"
        "QPushButton:hover { color: %2; background-color: %3; border-radius: 4px; }")
        .arg(tm.color("tab.inactiveForeground").name(),
             tm.color("badge.foreground").name(),
             tm.color("titleBar.buttonCloseHover").name()));

    auto leftBg = tm.color("editorLineNumber.background").name();
    auto listFg = tm.color("tab.inactiveForeground").name();
    auto listHoverBg = tm.color("list.hoverBackground").name();
    auto listHoverFg = tm.color("workbench.foreground").name();
    auto listSelBg = tm.color("menu.background").name();
    auto listSelFg = tm.color("tab.activeForeground").name();
    auto accent = tm.color("badge.background").name();
    auto handleBg = tm.color("scrollbarSlider.hoverBackground").name();
    auto borderColor = tm.color("panel.border").name();

    m_categoryList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background-color: %1;"
        "  border: none;"
        "  border-right: 1px solid %2;"
        "  color: %3;"
        "  font-size: 13px;"
        "  padding: 4px 0;"
        "  outline: none;"
        "}"
        "QListWidget::item { padding: 8px 16px; border: none; }"
        "QListWidget::item:hover { background-color: %4; color: %5; }"
        "QListWidget::item:selected {"
        "  background-color: %6; color: %7;"
        "  border-left: 3px solid %8; padding-left: 13px;"
        "}"
        "QScrollBar:vertical { background-color: %1; width: 10px; margin: 0; }"
        "QScrollBar::handle:vertical { background-color: %9; min-height: 30px; border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
        .arg(leftBg, borderColor, listFg, listHoverBg, listHoverFg,
             listSelBg, listSelFg, accent, handleBg));

    m_stackedWidget->setStyleSheet(QStringLiteral("background: %1;").arg(tm.color("menu.background").name()));

    // Refresh custom-styled widgets on each page
    if (m_resetThemeBtn) {
        m_resetThemeBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1; color: %2; border: 1px solid %3;"
            "  padding: 4px 12px; border-radius: 3px; font-size: 12px;"
            "}"
            "QPushButton:hover { background: %4; }")
            .arg(tm.color("input.background").name(),
                 tm.color("input.foreground").name(),
                 tm.color("input.border").name(),
                 tm.color("aiAssistant.actionButtonHoverBackground").name()));
    }
    if (m_shortcutsHeaderRow) {
        m_shortcutsHeaderRow->setStyleSheet(QStringLiteral("background: %1; border-top: 1px solid %2; border-right: 1px solid %2; border-bottom: none;")
            .arg(tm.color("activityBar.background").name(), tm.color("panel.border").name()));
    }
    if (m_shortcutsListContainer) {
        m_shortcutsListContainer->setStyleSheet(QStringLiteral("background: %1; border-top: 1px solid %2; border-right: 1px solid %2; border-bottom: 1px solid %2;")
            .arg(tm.color("menu.background").name(), tm.color("panel.border").name()));
    }
    if (m_shortcutsResetBtn) {
        m_shortcutsResetBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1; color: %2; border: 1px solid %3;"
            "  padding: 6px 16px; border-radius: 3px; font-size: 12px;"
            "}"
            "QPushButton:hover { background: %4; }")
            .arg(tm.color("input.background").name(),
                 tm.color("input.foreground").name(),
                 tm.color("input.border").name(),
                 tm.color("aiAssistant.actionButtonHoverBackground").name()));
    }
    if (m_aiSystemPromptEdit) {
        m_aiSystemPromptEdit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 3px;"
            "  padding: 4px;"
            "  font-size: 12px;"
            "}"
            "QTextEdit:focus {"
            "  border-color: %4;"
            "}")
            .arg(tm.color("input.background").name(),
                 tm.color("input.foreground").name(),
                 tm.color("input.border").name(),
                 tm.color("badge.background").name()));
    }

    auto refreshSmallButton = [&](QPushButton *btn, bool hasPadding) {
        if (!btn) return;
        QString padding = hasPadding ? QStringLiteral("padding: 4px 12px;") : QString();
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            "  %4 border-radius: 3px; font-size: 12px; }"
            "QPushButton:hover { background: %5; }")
            .arg(tm.color("input.background").name(),
                 tm.color("input.foreground").name(),
                 tm.color("input.border").name(),
                 padding,
                 tm.color("aiAssistant.actionButtonHoverBackground").name()));
    };
    refreshSmallButton(m_aiApiKeyToggleBtn, false);
    refreshSmallButton(m_clangdBrowseBtn, true);
    refreshSmallButton(m_pythonBrowseBtn, true);
    refreshSmallButton(m_openJudgePasswordToggleBtn, false);

    // Refresh all inner page content (scroll areas, labels, inputs)
    for (int i = 0; i < m_stackedWidget->count(); ++i) {
        refreshPageTree(m_stackedWidget->widget(i));
    }
}

void SettingsPanel::refreshPageTree(QWidget *w)
{
    auto &tm = ThemeManager::instance();

    if (auto *scrollArea = qobject_cast<QScrollArea*>(w)) {
        scrollArea->setStyleSheet(scrollAreaStyle());
        if (auto *content = scrollArea->widget()) {
            content->setStyleSheet(QStringLiteral("background: %1;").arg(tm.color("menu.background").name()));
        }
    } else if (auto *label = qobject_cast<QLabel*>(w)) {
        if (label->objectName() == QStringLiteral("shortcutsGroupLabel")) {
            label->setStyleSheet(QStringLiteral("color: %1; padding: 0px 8px; background: transparent;")
                .arg(tm.color("editor.foreground").name()));
            return; // font is set via QFont — only refresh color here
        }
        QString ss = label->styleSheet();
        if (ss.contains(QStringLiteral("font-size: 14px")))
            label->setStyleSheet(sectionLabelStyle());
        else if (ss.contains(QStringLiteral("font-size: 11px")))
            label->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; margin-bottom: 8px;")
                .arg(tm.color("tab.inactiveForeground").name()));
        else
            label->setStyleSheet(labelStyle());
    } else if (qobject_cast<QSpinBox*>(w) || qobject_cast<QLineEdit*>(w) || qobject_cast<QComboBox*>(w)) {
        w->setStyleSheet(inputStyle());
        // Do NOT recurse into internal children of QSpinBox/QComboBox
        // (e.g. the embedded QLineEdit inside QSpinBox / editable QComboBox).
        // Doing so would apply inputStyle (min-height:20px) to those internal
        // widgets, forcing the parent to grow taller than the intended 26px
        // and breaking label–control vertical alignment.
        if (qobject_cast<QSpinBox*>(w) || qobject_cast<QComboBox*>(w)) {
            if (qobject_cast<QComboBox*>(w))
                w->installEventFilter(this);
            return;
        }
    }

    for (auto *child : w->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        refreshPageTree(child);
    }
}

QLabel *SettingsPanel::createSectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setStyleSheet(sectionLabelStyle());
    return label;
}

// ============================================================
// Page: 编辑器 (Editor) — merged from editor, output, preview, search
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
    scrollArea->setStyleSheet(scrollAreaStyle());

    auto *content = new QWidget;
    content->setStyleSheet(QStringLiteral("background: %1;").arg(ThemeManager::instance().color("menu.background").name()));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    // ====================================================================
    // Section: 编辑
    // ====================================================================
    layout->addWidget(createSectionLabel(tr("编辑")));

    // ---- 编辑器字体 ----
    auto *fontRow = new QHBoxLayout;
    auto *fontLabel = new QLabel(tr("字体"));
    fontLabel->setStyleSheet(labelStyle());
    m_fontFamilyCombo = new FontDropdown;
    m_fontFamilyCombo->setEditable(false);
    m_fontFamilyCombo->setFixedWidth(180);
    m_fontFamilyCombo->setStyleSheet(inputStyle());

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
    auto *fontSizeLabel = new QLabel(tr("字号"));
    fontSizeLabel->setStyleSheet(labelStyle());
    m_fontSizeSpin = new ClampSpinBox;
    m_fontSizeSpin->setRange(8, 24);
    m_fontSizeSpin->setValue(cfg.editorFontSize());
    m_fontSizeSpin->setFixedWidth(100);
    m_fontSizeSpin->setStyleSheet(inputStyle());
    fontSizeRow->addWidget(fontSizeLabel);
    fontSizeRow->addStretch();
    fontSizeRow->addWidget(m_fontSizeSpin);
    layout->addLayout(fontSizeRow);

    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit editorSettingChanged("editor.font.size", val);
    });

    // ---- 缩进宽度 ----
    int indentDef = cfg.editorIndentWidth();
    auto *indentRow = new QHBoxLayout;
    auto *indentLabel = new QLabel(tr("缩进宽度"));
    indentLabel->setStyleSheet(labelStyle());
    m_indentWidthSpin = new ClampSpinBox;
    m_indentWidthSpin->setRange(1, 8);
    m_indentWidthSpin->setValue(indentDef);
    m_indentWidthSpin->setFixedWidth(100);
    m_indentWidthSpin->setStyleSheet(inputStyle());
    indentRow->addWidget(indentLabel);
    indentRow->addStretch();
    indentRow->addWidget(m_indentWidthSpin);
    layout->addLayout(indentRow);

    connect(m_indentWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit editorSettingChanged("editor.indent_width", val);
    });

    // ---- Markdown 缩进宽度 ----
    int mdIndentDef = cfg.editorMarkdownIndentWidth();
    auto *mdIndentRow = new QHBoxLayout;
    auto *mdIndentLabel = new QLabel(tr("MD 缩进宽度"));
    mdIndentLabel->setStyleSheet(labelStyle());
    m_markdownIndentWidthSpin = new ClampSpinBox;
    m_markdownIndentWidthSpin->setRange(1, 8);
    m_markdownIndentWidthSpin->setValue(mdIndentDef);
    m_markdownIndentWidthSpin->setFixedWidth(100);
    m_markdownIndentWidthSpin->setStyleSheet(inputStyle());
    mdIndentRow->addWidget(mdIndentLabel);
    mdIndentRow->addStretch();
    mdIndentRow->addWidget(m_markdownIndentWidthSpin);
    layout->addLayout(mdIndentRow);

    connect(m_markdownIndentWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit editorSettingChanged("editor.markdown_indent_width", val);
    });

    // ---- 默认缩放 ----
    auto *zoomRow = new QHBoxLayout;
    auto *zoomLabel = new QLabel(tr("默认缩放（%）"));
    zoomLabel->setStyleSheet(labelStyle());
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
    m_fontSizeEdit->setFixedWidth(100);
    m_fontSizeEdit->setStyleSheet(inputStyle());
    zoomRow->addWidget(m_fontSizeEdit);
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

    // ====================================================================
    // Section: 自动保存
    // ====================================================================
    layout->addSpacing(12);
    layout->addWidget(createSectionLabel(tr("自动保存")));

    // ---- 自动保存开关 ----
    auto *autoSaveRow = new QHBoxLayout;
    auto *autoSaveLabel = new QLabel(tr("自动保存"));
    autoSaveLabel->setStyleSheet(labelStyle());

    m_autoSaveToggle = new ToggleSwitch;
    m_autoSaveToggle->setChecked(true);

    auto *autoSaveStateLabel = new QLabel(tr("开"));
    autoSaveStateLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(ThemeManager::instance().color("badge.background").name()));

    m_autoSaveToggle->onToggled = [this, autoSaveStateLabel](bool checked) {
        autoSaveStateLabel->setText(checked ? tr("开") : tr("关"));
        autoSaveStateLabel->setStyleSheet(QString("color: %1; font-size: 13px;")
            .arg(checked ? ThemeManager::instance().color("badge.background").name() : ThemeManager::instance().color("tab.inactiveForeground").name()));
        emit editorSettingChanged("auto_save.enabled", checked);
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

    // ---- 保存间隔 ----
    auto *intervalRow = new QHBoxLayout;
    auto *intervalLabel = new QLabel(tr("保存间隔（秒）"));
    intervalLabel->setStyleSheet(labelStyle());
    m_autoSaveIntervalSpin = new ClampSpinBox;
    m_autoSaveIntervalSpin->setRange(1, 300);
    m_autoSaveIntervalSpin->setValue(cfg.autoSaveIntervalMs() / 1000);
    m_autoSaveIntervalSpin->setFixedWidth(100);
    m_autoSaveIntervalSpin->setStyleSheet(inputStyle());
    intervalRow->addWidget(intervalLabel);
    intervalRow->addStretch();
    intervalRow->addWidget(m_autoSaveIntervalSpin);
    layout->addLayout(intervalRow);

    connect(m_autoSaveIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit editorSettingChanged("auto_save.interval_ms", val * 1000);
    });

    // ====================================================================
    // Section: 输出面板
    // ====================================================================
    layout->addSpacing(12);
    layout->addWidget(createSectionLabel(tr("输出面板")));

    // ---- 输出字体 ----
    auto *outFontRow = new QHBoxLayout;
    auto *outFontLabel = new QLabel(tr("字体"));
    outFontLabel->setStyleSheet(labelStyle());
    m_outputFontFamilyCombo = new FontDropdown;
    m_outputFontFamilyCombo->setEditable(false);
    m_outputFontFamilyCombo->setFixedWidth(180);
    m_outputFontFamilyCombo->setStyleSheet(inputStyle());
    m_outputFontFamilyCombo->addItems(families);
    int outFontIdx = m_outputFontFamilyCombo->findText(cfg.outputPanelFontFamily());
    if (outFontIdx >= 0) m_outputFontFamilyCombo->setCurrentIndex(outFontIdx);
    outFontRow->addWidget(outFontLabel);
    outFontRow->addStretch();
    outFontRow->addWidget(m_outputFontFamilyCombo);
    layout->addLayout(outFontRow);

    connect(m_outputFontFamilyCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        emit outputPanelSettingChanged("output_panel.font.family", text);
    });

    // ---- 输出字号 ----
    auto *outSizeRow = new QHBoxLayout;
    auto *outSizeLabel = new QLabel(tr("字号"));
    outSizeLabel->setStyleSheet(labelStyle());
    m_outputFontSizeSpin = new ClampSpinBox;
    m_outputFontSizeSpin->setRange(8, 24);
    m_outputFontSizeSpin->setValue(cfg.outputPanelFontSize());
    m_outputFontSizeSpin->setFixedWidth(100);
    m_outputFontSizeSpin->setStyleSheet(inputStyle());
    outSizeRow->addWidget(outSizeLabel);
    outSizeRow->addStretch();
    outSizeRow->addWidget(m_outputFontSizeSpin);
    layout->addLayout(outSizeRow);

    connect(m_outputFontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit outputPanelSettingChanged("output_panel.font.size", val);
    });

    // ---- 最大行数 ----
    auto *maxRow = new QHBoxLayout;
    auto *maxLabel = new QLabel(tr("最大行数"));
    maxLabel->setStyleSheet(labelStyle());
    m_outputMaxBlocksSpin = new ClampSpinBox;
    m_outputMaxBlocksSpin->setRange(100, 100000);
    m_outputMaxBlocksSpin->setSingleStep(500);
    m_outputMaxBlocksSpin->setValue(cfg.outputPanelMaxBlocks());
    m_outputMaxBlocksSpin->setFixedWidth(100);
    m_outputMaxBlocksSpin->setStyleSheet(inputStyle());
    maxRow->addWidget(maxLabel);
    maxRow->addStretch();
    maxRow->addWidget(m_outputMaxBlocksSpin);
    layout->addLayout(maxRow);

    connect(m_outputMaxBlocksSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit outputPanelSettingChanged("output_panel.max_blocks", val);
    });

    // ====================================================================
    // Section: 预览
    // ====================================================================
    layout->addSpacing(12);
    layout->addWidget(createSectionLabel(tr("预览")));

    // ---- 分屏防抖 ----
    auto *debounceRow = new QHBoxLayout;
    auto *debounceLabel = new QLabel(tr("分屏防抖（ms）"));
    debounceLabel->setStyleSheet(labelStyle());
    m_previewDebounceSpin = new ClampSpinBox;
    m_previewDebounceSpin->setRange(100, 2000);
    m_previewDebounceSpin->setSingleStep(50);
    m_previewDebounceSpin->setValue(cfg.previewSplitDebounceMs());
    m_previewDebounceSpin->setFixedWidth(100);
    m_previewDebounceSpin->setStyleSheet(inputStyle());
    debounceRow->addWidget(debounceLabel);
    debounceRow->addStretch();
    debounceRow->addWidget(m_previewDebounceSpin);
    layout->addLayout(debounceRow);

    connect(m_previewDebounceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit previewSettingChanged("preview.split_debounce_ms", val);
    });

    // ---- 分屏比例 ----
    auto *ratioRow = new QHBoxLayout;
    auto *ratioLabel = new QLabel(tr("分屏比例（%）"));
    ratioLabel->setStyleSheet(labelStyle());
    m_previewRatioSpin = new ClampSpinBox;
    m_previewRatioSpin->setRange(30, 70);
    m_previewRatioSpin->setValue(cfg.previewSplitPreviewRatio());
    m_previewRatioSpin->setFixedWidth(100);
    m_previewRatioSpin->setStyleSheet(inputStyle());
    ratioRow->addWidget(ratioLabel);
    ratioRow->addStretch();
    ratioRow->addWidget(m_previewRatioSpin);
    layout->addLayout(ratioRow);

    connect(m_previewRatioSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit previewSettingChanged("preview.split_preview_ratio", val);
    });

    // ====================================================================
    // Section: 搜索
    // ====================================================================
    layout->addSpacing(12);
    layout->addWidget(createSectionLabel(tr("搜索")));

    // ---- 每文件最大匹配 ----
    auto *perFileRow = new QHBoxLayout;
    auto *perFileLabel = new QLabel(tr("每文件匹配数"));
    perFileLabel->setStyleSheet(labelStyle());
    m_searchPerFileSpin = new ClampSpinBox;
    m_searchPerFileSpin->setRange(1, 50);
    m_searchPerFileSpin->setValue(cfg.searchMaxPerFile());
    m_searchPerFileSpin->setFixedWidth(100);
    m_searchPerFileSpin->setStyleSheet(inputStyle());
    perFileRow->addWidget(perFileLabel);
    perFileRow->addStretch();
    perFileRow->addWidget(m_searchPerFileSpin);
    layout->addLayout(perFileRow);

    connect(m_searchPerFileSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit searchSettingChanged("search_panel.max_per_file", val);
    });

    // ---- 最大结果总数 ----
    auto *totalRow = new QHBoxLayout;
    auto *totalLabel = new QLabel(tr("最大结果总数"));
    totalLabel->setStyleSheet(labelStyle());
    m_searchTotalSpin = new ClampSpinBox;
    m_searchTotalSpin->setRange(50, 2000);
    m_searchTotalSpin->setSingleStep(50);
    m_searchTotalSpin->setValue(cfg.searchMaxTotalResults());
    m_searchTotalSpin->setFixedWidth(100);
    m_searchTotalSpin->setStyleSheet(inputStyle());
    totalRow->addWidget(totalLabel);
    totalRow->addStretch();
    totalRow->addWidget(m_searchTotalSpin);
    layout->addLayout(totalRow);

    connect(m_searchTotalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit searchSettingChanged("search_panel.max_total_results", val);
    });

    // ---- 片段最大长度 ----
    auto *snippetRow = new QHBoxLayout;
    auto *snippetLabel = new QLabel(tr("片段最大长度"));
    snippetLabel->setStyleSheet(labelStyle());
    m_searchSnippetSpin = new ClampSpinBox;
    m_searchSnippetSpin->setRange(50, 500);
    m_searchSnippetSpin->setSingleStep(10);
    m_searchSnippetSpin->setValue(cfg.searchSnippetMaxLength());
    m_searchSnippetSpin->setFixedWidth(100);
    m_searchSnippetSpin->setStyleSheet(inputStyle());
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
    scrollArea->setStyleSheet(scrollAreaStyle());

    auto *content = new QWidget;
    content->setStyleSheet(QStringLiteral("background: %1;").arg(ThemeManager::instance().color("menu.background").name()));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    layout->addWidget(createSectionLabel(tr("主题")));

    // ---- 主题选择 ----
    auto *themeRow = new QHBoxLayout;
    auto *themeLabel = new QLabel(tr("主题"));
    themeLabel->setStyleSheet(labelStyle());
    m_themeCombo = new QComboBox;
    m_themeCombo->setStyleSheet(inputStyle());
    m_themeCombo->setFixedWidth(180);

    auto &tm = ThemeManager::instance();
    m_themeCombo->addItems(tm.availableThemes());
    m_themeCombo->setCurrentText(tm.currentThemeName());

    connect(m_themeCombo, &QComboBox::currentTextChanged, this, [this](const QString &name) {
        ThemeManager::instance().loadTheme(name);
        SettingsManager::instance().setSettingOverride("appearance.theme", name);
    });
    connect(&tm, &ThemeManager::themeChanged, this, [this]() {
        m_themeCombo->setCurrentText(ThemeManager::instance().currentThemeName());
        refreshStyle();
    });

    m_resetThemeBtn = new QPushButton(tr("恢复主题默认值"));
    m_resetThemeBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1; color: %2; border: 1px solid %3;"
        "  padding: 4px 12px; border-radius: 3px; font-size: 12px;"
        "}"
        "QPushButton:hover { background: %4; }")
        .arg(ThemeManager::instance().color("input.background").name(),
             ThemeManager::instance().color("input.foreground").name(),
             ThemeManager::instance().color("input.border").name(),
             ThemeManager::instance().color("aiAssistant.actionButtonHoverBackground").name())
    );
    connect(m_resetThemeBtn, &QPushButton::clicked, this, [this]() {
        auto &tm = ThemeManager::instance();
        tm.clearOverrides();
        tm.loadTheme(tm.currentThemeName());

        // Keep SettingsManager in sync so panel values match on next open
        auto &sm = SettingsManager::instance();
        for (const auto &k : sm.allOverrideKeys()) {
            if (k.startsWith(QStringLiteral("appearance.colors.")))
                sm.removeSettingOverride(k);
        }
        syncFromSettings(sm);
    });

    themeRow->addWidget(themeLabel);
    themeRow->addStretch();
    themeRow->addWidget(m_themeCombo);
    themeRow->addWidget(m_resetThemeBtn);
    layout->addLayout(themeRow);

    layout->addSpacing(12);

    layout->addWidget(createSectionLabel(tr("文件树")));

    // ---- 文件树条目高度 ----
    auto *treeItemHeightRow = new QHBoxLayout;
    auto *treeItemHeightLabel = new QLabel(tr("条目行高（px）"));
    treeItemHeightLabel->setStyleSheet(labelStyle());
    m_fileTreeItemHeightSpin = new ClampSpinBox;
    m_fileTreeItemHeightSpin->setRange(24, 32);
    m_fileTreeItemHeightSpin->setValue(cfg.editorFileTreeItemHeight());
    m_fileTreeItemHeightSpin->setFixedWidth(100);
    m_fileTreeItemHeightSpin->setStyleSheet(inputStyle());
    treeItemHeightRow->addWidget(treeItemHeightLabel);
    treeItemHeightRow->addStretch();
    treeItemHeightRow->addWidget(m_fileTreeItemHeightSpin);
    layout->addLayout(treeItemHeightRow);

    connect(m_fileTreeItemHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit appearanceSettingChanged("editor.file_tree_item_height", val);
    });

    layout->addSpacing(4);
    layout->addWidget(createSectionLabel(tr("标签页")));

    // ---- 等宽标签页 ----
    auto *equalWidthRow = new QHBoxLayout;
    auto *equalWidthLabel = new QLabel(tr("等宽标签页"));
    equalWidthLabel->setStyleSheet(labelStyle());
    m_equalWidthTabToggle = new ToggleSwitch;
    m_equalWidthTabToggle->setChecked(
        SettingsManager::instance().value("editor.equal_width_tab", false).toBool());
    m_equalWidthTabToggle->onToggled = [this](bool checked) {
        emit appearanceSettingChanged("editor.equal_width_tab", checked);
    };
    equalWidthRow->addWidget(equalWidthLabel);
    equalWidthRow->addStretch();
    equalWidthRow->addWidget(m_equalWidthTabToggle);
    layout->addLayout(equalWidthRow);

    // ====================================================================
    // Collapsible Color Sections
    // ====================================================================
    layout->addSpacing(8);
    m_colorControls.clear();

    // Helper: create a single color picker row inside a section layout
    auto addColorRow = [&](QVBoxLayout *parent, const QString &label,
                           const QString &configKey, const QColor &defaultColor)
    {
        auto *row = new QHBoxLayout;
        auto *lbl = new QLabel(label);
        lbl->setStyleSheet(labelStyle());

        QString currentHex = sm.value(configKey, defaultColor.name()).toString();

        auto *colorBtn = new QPushButton;
        colorBtn->setFixedSize(24, 24);
        QString btnBorder = tm.color("input.border").name();
        QString btnActive = tm.color("badge.background").name();
        colorBtn->setStyleSheet(
            QStringLiteral(
                "QPushButton { background-color: %1; border: 1px solid %2; border-radius: 4px; }"
                "QPushButton:hover { border-color: %3; }"
            ).arg(currentHex, btnBorder, btnActive));

        auto *colorPreview = new QLabel;
        colorPreview->setFixedWidth(80);
        colorPreview->setText(currentHex);
        colorPreview->setStyleSheet(
            QStringLiteral("color: %1; font-size: 11px; padding: 2px 4px; "
                           "background-color: %2; border: 1px solid %3; border-radius: 3px;")
            .arg(tm.color("tab.inactiveForeground").name(),
                 tm.color("input.background").name(),
                 tm.color("input.border").name()));

        connect(colorBtn, &QPushButton::clicked, this, [this, colorBtn, colorPreview, configKey, defaultColor]() {
            auto &sm2 = SettingsManager::instance();
            QString curHex = sm2.value(configKey, defaultColor.name()).toString();
            QColor curCol = QColor(curHex);
            QColor chosen = QColorDialog::getColor(curCol.isValid() ? curCol : defaultColor,
                                                    this, tr("选择颜色"));
            if (chosen.isValid()) {
                QString hex = chosen.name();
                auto &ctm = ThemeManager::instance();
                colorBtn->setStyleSheet(
                    QStringLiteral(
                        "QPushButton { background-color: %1; border: 1px solid %2; border-radius: 4px; }"
                        "QPushButton:hover { border-color: %3; }"
                    ).arg(hex, ctm.color("input.border").name(), ctm.color("badge.background").name()));
                colorPreview->setText(hex);
                emit appearanceSettingChanged(configKey, hex);
            }
        });

        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(colorPreview);
        row->addWidget(colorBtn);
        parent->addLayout(row);

        m_colorControls.append({colorBtn, colorPreview, configKey, defaultColor});
    };

    // Helper: create a collapsible section with QToolButton toggle
    auto createSection = [&](const QString &title, bool defaultExpanded)
    {
        auto *btn = new QToolButton;
        btn->setCheckable(true);
        btn->setChecked(defaultExpanded);
        btn->setText(title);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setArrowType(defaultExpanded ? Qt::DownArrow : Qt::RightArrow);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { color: %1; font-size: 13px; font-weight: bold; border: none; padding: 2px 0; }"
            "QToolButton:hover { color: %2; }")
            .arg(tm.color("tab.activeForeground").name(),
                 tm.color("badge.background").name()));

        auto *content = new QWidget;
        content->setVisible(defaultExpanded);
        auto *contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(8, 4, 0, 4);
        contentLayout->setSpacing(4);

        QObject::connect(btn, &QToolButton::toggled, content, &QWidget::setVisible);
        QObject::connect(btn, &QToolButton::toggled, btn, [btn](bool checked) {
            btn->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        });

        return QPair<QToolButton*, QVBoxLayout*>(btn, contentLayout);
    };

    // ---- 编辑器色 (6 colors) ----
    {
        auto [btn, cl] = createSection(tr("编辑器色"), true);
        layout->addWidget(btn);

        addColorRow(cl, tr("编辑器背景"),            "appearance.colors.editor.background",      cfg.editorBackground());
        addColorRow(cl, tr("编辑器前景（文字）"),     "appearance.colors.editor.foreground",      cfg.editorForeground());
        addColorRow(cl, tr("选中背景"),              "appearance.colors.editor.selection",       cfg.editorSelection());
        addColorRow(cl, tr("当前行高亮"),             "appearance.colors.current_line.highlight", cfg.currentLineHighlight());
        addColorRow(cl, tr("行号区域背景"),           "appearance.colors.line_number.background", cfg.lineNumberBackground());
        addColorRow(cl, tr("行号文字颜色"),           "appearance.colors.line_number.foreground", cfg.lineNumberForeground());

        layout->addWidget(cl->parentWidget());
    }

    // ---- 语法高亮 (8 colors) ----
    {
        auto [btn, cl] = createSection(tr("语法高亮"), false);
        layout->addWidget(btn);

        addColorRow(cl, tr("关键字"),       "appearance.colors.syntax_highlight.keywords",         cfg.syntaxKeywords());
        addColorRow(cl, tr("控制关键字"),   "appearance.colors.syntax_highlight.controlKeywords",  cfg.syntaxControlKeywords());
        addColorRow(cl, tr("预处理指令"),   "appearance.colors.syntax_highlight.preprocessor",     cfg.syntaxPreprocessor());
        addColorRow(cl, tr("类型"),         "appearance.colors.syntax_highlight.types",            cfg.syntaxTypes());
        addColorRow(cl, tr("数字"),         "appearance.colors.syntax_highlight.numbers",          cfg.syntaxNumbers());
        addColorRow(cl, tr("字符串"),       "appearance.colors.syntax_highlight.strings",          cfg.syntaxStrings());
        addColorRow(cl, tr("注释"),         "appearance.colors.syntax_highlight.comments",         cfg.syntaxComments());
        addColorRow(cl, tr("函数"),         "appearance.colors.syntax_highlight.functions",        cfg.syntaxFunctions());
        addColorRow(cl, tr("参数"),         "appearance.colors.syntax_highlight.parameters",       cfg.syntaxParameters());
        addColorRow(cl, tr("Python 装饰器"), "appearance.colors.syntax_highlight.python_decorators", cfg.syntaxPythonDecorators());
        addColorRow(cl, tr("Python self/cls"), "appearance.colors.syntax_highlight.python_self_cls", cfg.syntaxPythonSelfCls());

        layout->addWidget(cl->parentWidget());
    }

    // ---- 输出面板 (4 colors) ----
    {
        auto [btn, cl] = createSection(tr("输出面板"), false);
        layout->addWidget(btn);

        addColorRow(cl, tr("背景"),       "appearance.colors.output_panel.background", cfg.outputPanelBackground());
        addColorRow(cl, tr("前景"),       "appearance.colors.output_panel.foreground", cfg.outputPanelForeground());
        addColorRow(cl, tr("选中色"),     "appearance.colors.output_panel.selection",  cfg.outputPanelSelection());
        addColorRow(cl, tr("错误输出"),   "appearance.colors.output_panel.stderr",     cfg.outputStderr());

        layout->addWidget(cl->parentWidget());
    }

    // ---- 搜索高亮 (2 colors) ----
    {
        auto [btn, cl] = createSection(tr("搜索高亮"), false);
        layout->addWidget(btn);

        addColorRow(cl, tr("高亮背景"), "appearance.colors.search.highlight_background", cfg.searchHighlightBackground());
        addColorRow(cl, tr("高亮文字"), "appearance.colors.search.highlight_foreground", cfg.searchHighlightForeground());

        layout->addWidget(cl->parentWidget());
    }

    // ---- 预览 (2 colors) ----
    {
        auto [btn, cl] = createSection(tr("预览"), false);
        layout->addWidget(btn);

        addColorRow(cl, tr("容器背景"),         "appearance.colors.preview.container_background",   cfg.previewContainerBackground());
        addColorRow(cl, tr("WebEngine 背景"),   "appearance.colors.preview.webengine_background",   cfg.previewWebEngineBackground());

        layout->addWidget(cl->parentWidget());
    }

    // ---- Judge 状态 (8 colors) ----
    {
        auto [btn, cl] = createSection(tr("Judge 状态"), false);
        layout->addWidget(btn);

        addColorRow(cl, tr("AC"),  "appearance.colors.judge_status.ac",  cfg.judgeColorAc());
        addColorRow(cl, tr("WA"),  "appearance.colors.judge_status.wa",  cfg.judgeColorWa());
        addColorRow(cl, tr("TLE"), "appearance.colors.judge_status.tle", cfg.judgeColorTle());
        addColorRow(cl, tr("MLE"), "appearance.colors.judge_status.mle", cfg.judgeColorMle());
        addColorRow(cl, tr("RE"),  "appearance.colors.judge_status.re",  cfg.judgeColorRe());
        addColorRow(cl, tr("PE"),  "appearance.colors.judge_status.pe",  cfg.judgeColorPe());
        addColorRow(cl, tr("OLE"), "appearance.colors.judge_status.ole", cfg.judgeColorOle());
        addColorRow(cl, tr("CE"),  "appearance.colors.judge_status.ce",  cfg.judgeColorCe());

        layout->addWidget(cl->parentWidget());
    }

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
    scrollArea->setStyleSheet(scrollAreaStyle());

    auto *content = new QWidget;
    content->setStyleSheet(QStringLiteral("background: %1;").arg(ThemeManager::instance().color("menu.background").name()));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(2);

    layout->addWidget(createSectionLabel(tr("快捷键")));

    // 说明文字
    auto *descLabel = new QLabel(tr("点击快捷键可重新录制，按 Delete/Backspace 清除，按 Escape 取消"));
    descLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; margin-bottom: 8px;").arg(ThemeManager::instance().color("tab.inactiveForeground").name()));
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
    m_shortcutsHeaderRow = new QWidget;
    m_shortcutsHeaderRow->setStyleSheet(QStringLiteral("background: %1; border-top: 1px solid %2; border-right: 1px solid %2; border-bottom: none;").arg(ThemeManager::instance().color("activityBar.background").name(), ThemeManager::instance().color("panel.border").name()));
    auto *headerLayout = new QHBoxLayout(m_shortcutsHeaderRow);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    auto *nameHeader = new QLabel(tr("操作"));
    QString headerFg = ThemeManager::instance().color("tab.inactiveForeground").name();
    nameHeader->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; font-size: 12px;").arg(headerFg));
    auto *keyHeader = new QLabel(tr("快捷键"));
    keyHeader->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; font-size: 12px;").arg(headerFg));
    headerLayout->addWidget(nameHeader, 1);
    headerLayout->addWidget(keyHeader, 1);
    layout->addWidget(m_shortcutsHeaderRow);

    // Build a container for the list
    m_shortcutsListContainer = new QWidget;
    m_shortcutsListContainer->setStyleSheet(QStringLiteral("background: %1; border-top: 1px solid %2; border-right: 1px solid %2; border-bottom: 1px solid %2;").arg(ThemeManager::instance().color("menu.background").name(), ThemeManager::instance().color("panel.border").name()));
    auto *listLayout = new QVBoxLayout(m_shortcutsListContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    // Conflict indicator update (Minecraft-style: allow conflicts, mark in red)
    auto updateConflictIndicators = [this]() {
        QMap<QString, int> seqCount;
        for (auto it = m_keyRecorders.constBegin(); it != m_keyRecorders.constEnd(); ++it) {
            QString seq = it.value()->keySequence();
            if (!seq.isEmpty())
                seqCount[seq]++;
        }
        for (auto it = m_keyRecorders.constBegin(); it != m_keyRecorders.constEnd(); ++it) {
            QString seq = it.value()->keySequence();
            it.value()->setConflict(!seq.isEmpty() && seqCount.value(seq) > 1);
        }
    };

    m_keyRecorders.clear();

    // Group definitions for visual organization
    struct GroupMarker { int index; QString name; };
    GroupMarker markers[] = {
        {0,   tr("通用")},
        {25,  tr("CodeEditor")},
        {28,  tr("SmdEditor")},
        {35,  tr("SmdEditor 命令模式")},
        {38,  tr("输出面板")},
    };

    int markerIdx = 0;
    constexpr int kNumItems = sizeof(items) / sizeof(items[0]);
    for (int i = 0; i < kNumItems; ++i) {
        const auto &item = items[i];

        // Insert group header before the first item of each group
        if (markerIdx < 5 && i == markers[markerIdx].index) {
            listLayout->addSpacing(16);
            auto *groupLabel = new QLabel(markers[markerIdx].name);
            groupLabel->setObjectName("shortcutsGroupLabel");
            QFont gf = groupLabel->font();
            gf.setPixelSize(18);
            gf.setBold(true);
            groupLabel->setFont(gf);
            groupLabel->setStyleSheet(QStringLiteral(
                "color: %1; padding: 0px 8px; background: transparent;")
                .arg(ThemeManager::instance().color("editor.foreground").name()));
            listLayout->addWidget(groupLabel);
            listLayout->addSpacing(4);
            ++markerIdx;
        }


        auto *row = new QWidget;
        row->setFixedHeight(32);
        row->setStyleSheet("background: transparent;");

        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(8, 0, 8, 0);
        rowLayout->setSpacing(8);

        auto *label = new QLabel(item.displayName);
        label->setStyleSheet(labelStyle());
        rowLayout->addWidget(label, 1);

        QString shortcutStr = cfg.shortcut(item.configKey, "");
        auto *recorder = new KeyRecorder(item.configKey, shortcutStr);
        rowLayout->addWidget(recorder, 1);

        m_keyRecorders[item.configKey] = recorder;

        connect(recorder, &KeyRecorder::keySequenceCaptured,
                this, [this, recorder, updateConflictIndicators](const QString &actionKey, const QKeySequence &ks) {
            QString newText = ks.toString(QKeySequence::NativeText);
            recorder->setKeySequence(newText);
            emit shortcutChanged(actionKey, newText);
            updateConflictIndicators();
        });

        // Separator before row (skip before first item of each group)
        if (i > 0) {
            bool isGroupStart = false;
            for (int m = 0; m < 5; ++m) {
                if (i == markers[m].index) {
                    isGroupStart = true;
                    break;
                }
            }
            if (!isGroupStart) {
                auto *sep = new QFrame;
                sep->setFrameShape(QFrame::HLine);
                sep->setStyleSheet(QStringLiteral("color: %1;").arg(ThemeManager::instance().color("panel.border").name()));
                sep->setFixedHeight(1);
                listLayout->addWidget(sep);
            }
        }

        listLayout->addWidget(row);
    }

    // Mark any pre-existing conflicts from saved settings
    updateConflictIndicators();

    layout->addWidget(m_shortcutsListContainer, 1);

    // Reset all button
    m_shortcutsResetBtn = new QPushButton(tr("恢复默认"));
    m_shortcutsResetBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1; color: %2; border: 1px solid %3;"
        "  padding: 6px 16px; border-radius: 3px; font-size: 12px;"
        "}"
        "QPushButton:hover { background: %4; }")
        .arg(ThemeManager::instance().color("input.background").name(),
             ThemeManager::instance().color("input.foreground").name(),
             ThemeManager::instance().color("input.border").name(),
             ThemeManager::instance().color("aiAssistant.actionButtonHoverBackground").name())
    );
    m_shortcutsResetBtn->setFixedWidth(120);
    connect(m_shortcutsResetBtn, &QPushButton::clicked, this, [this, updateConflictIndicators]() {
        const auto &cfg2 = ConfigManager::instance();
        for (auto it = m_keyRecorders.begin(); it != m_keyRecorders.end(); ++it) {
            QString defaultVal = cfg2.shortcut(it.key(), "");
            it.value()->setKeySequence(defaultVal);
            emit shortcutChanged(it.key(), defaultVal);
        }
        updateConflictIndicators();
    });
    layout->addWidget(m_shortcutsResetBtn, 0, Qt::AlignLeft);

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
    scrollArea->setStyleSheet(scrollAreaStyle());

    auto *content = new QWidget;
    content->setStyleSheet(QStringLiteral("background: %1;").arg(ThemeManager::instance().color("menu.background").name()));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    layout->addWidget(createSectionLabel(tr("AI 服务")));

    // ---- API 类型 ----
    auto *providerRow = new QHBoxLayout;
    auto *providerLabel = new QLabel(tr("API 类型"));
    providerLabel->setStyleSheet(labelStyle());
    m_aiProviderCombo = new QComboBox;
    m_aiProviderCombo->addItems({QStringLiteral("Anthropic"), QStringLiteral("OpenAI")});
    m_aiProviderCombo->setFixedWidth(180);
    m_aiProviderCombo->setStyleSheet(inputStyle());
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
    endpointLabel->setStyleSheet(labelStyle());
    m_aiEndpointEdit = new QLineEdit;
    m_aiEndpointEdit->setPlaceholderText(QStringLiteral("https://api.deepseek.com/v1"));
    m_aiEndpointEdit->setStyleSheet(inputStyle());
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
    keyLabel->setStyleSheet(labelStyle());
    m_aiApiKeyEdit = new QLineEdit;
    m_aiApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_aiApiKeyEdit->setPlaceholderText(tr("输入 API Key"));
    m_aiApiKeyEdit->setStyleSheet(inputStyle());
    m_aiApiKeyToggleBtn = new QPushButton(tr("显示"));
    m_aiApiKeyToggleBtn->setFixedSize(50, 28);
    m_aiApiKeyToggleBtn->setCursor(Qt::PointingHandCursor);
    m_aiApiKeyToggleBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background: %4; }")
        .arg(ThemeManager::instance().color("input.background").name(),
             ThemeManager::instance().color("input.foreground").name(),
             ThemeManager::instance().color("input.border").name(),
             ThemeManager::instance().color("aiAssistant.actionButtonHoverBackground").name()));
    keyRow->addWidget(keyLabel);
    keyRow->addStretch();
    keyRow->addWidget(m_aiApiKeyEdit, 1);
    keyRow->addWidget(m_aiApiKeyToggleBtn);
    layout->addLayout(keyRow);

    connect(m_aiApiKeyEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit aiSettingChanged("ai.api_key", text);
    });
    connect(m_aiApiKeyToggleBtn, &QPushButton::clicked, this, [this]() {
        bool hidden = (m_aiApiKeyEdit->echoMode() == QLineEdit::Password);
        m_aiApiKeyEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        m_aiApiKeyToggleBtn->setText(hidden ? tr("隐藏") : tr("显示"));
    });

    // ---- 模型 ----
    auto *modelRow = new QHBoxLayout;
    auto *modelLabel = new QLabel(tr("模型"));
    modelLabel->setStyleSheet(labelStyle());
    m_aiModelEdit = new QLineEdit;
    m_aiModelEdit->setPlaceholderText(QStringLiteral("deepseek-v4-flash"));
    m_aiModelEdit->setStyleSheet(inputStyle());
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
    tokensLabel->setStyleSheet(labelStyle());
    m_aiMaxTokensSpin = new ClampSpinBox;
    m_aiMaxTokensSpin->setRange(256, 16384);
    m_aiMaxTokensSpin->setSingleStep(256);
    m_aiMaxTokensSpin->setValue(cfg.aiMaxTokens());
    m_aiMaxTokensSpin->setFixedWidth(100);
    m_aiMaxTokensSpin->setStyleSheet(inputStyle());
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
    promptLabel->setStyleSheet(labelStyle());
    layout->addWidget(promptLabel);

    m_aiSystemPromptEdit = new QTextEdit;
    m_aiSystemPromptEdit->setPlaceholderText(tr("可选的系统提示词，设定 AI 助手的角色和行为..."));
    m_aiSystemPromptEdit->setFixedHeight(120);
    m_aiSystemPromptEdit->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 3px;"
        "  padding: 4px;"
        "  font-size: 12px;"
        "}"
        "QTextEdit:focus {"
        "  border-color: %4;"
        "}")
        .arg(ThemeManager::instance().color("input.background").name(),
             ThemeManager::instance().color("input.foreground").name(),
             ThemeManager::instance().color("input.border").name(),
             ThemeManager::instance().color("badge.background").name())
    );
    layout->addWidget(m_aiSystemPromptEdit);

    m_aiPromptDebounceTimer = new QTimer(this);
    m_aiPromptDebounceTimer->setSingleShot(true);
    m_aiPromptDebounceTimer->setInterval(300);
    connect(m_aiPromptDebounceTimer, &QTimer::timeout, this, [this]() {
        emit aiSettingChanged("ai.system_prompt", m_aiSystemPromptEdit->toPlainText());
    });
    connect(m_aiSystemPromptEdit, &QTextEdit::textChanged, this, [this]() {
        m_aiPromptDebounceTimer->start();
    });
    // 焦点离开时立即 emit 最终值
    m_aiSystemPromptEdit->installEventFilter(this);

    layout->addStretch();
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    return page;
}

// ============================================================
// Page: 工具 (Tools)
// ============================================================
QWidget *SettingsPanel::createToolsPage()
{
    const auto &cfg = ConfigManager::instance();
    auto *page = new QWidget;
    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(scrollAreaStyle());

    auto *content = new QWidget;
    content->setStyleSheet(QStringLiteral("background: %1;").arg(ThemeManager::instance().color("menu.background").name()));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(8);

    // ====================================================================
    // Section: 语言服务
    // ====================================================================
    layout->addWidget(createSectionLabel(tr("语言服务")));

    // ---- clangd 路径 ----
    auto *clangdPathRow = new QHBoxLayout;
    auto *clangdPathLabel = new QLabel(tr("clangd 路径"));
    clangdPathLabel->setStyleSheet(labelStyle());
    m_clangdPathEdit = new QLineEdit;
    m_clangdPathEdit->setPlaceholderText(tr("留空则从 PATH 查找"));
    m_clangdPathEdit->setStyleSheet(inputStyle());
    m_clangdPathEdit->setText(cfg.toolClangdPath());
    m_clangdBrowseBtn = new QPushButton(tr("浏览…"));
    m_clangdBrowseBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "padding: 4px 12px; border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background: %4; }")
        .arg(ThemeManager::instance().color("input.background").name(),
             ThemeManager::instance().color("input.foreground").name(),
             ThemeManager::instance().color("input.border").name(),
             ThemeManager::instance().color("aiAssistant.actionButtonHoverBackground").name()));
    clangdPathRow->addWidget(clangdPathLabel);
    clangdPathRow->addStretch();
    clangdPathRow->addWidget(m_clangdPathEdit, 1);
    clangdPathRow->addWidget(m_clangdBrowseBtn);
    layout->addLayout(clangdPathRow);

    connect(m_clangdPathEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit toolSettingChanged("tools.clangd.path", text);
    });
    connect(m_clangdBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("选择 clangd 可执行文件"));
        if (!path.isEmpty())
            m_clangdPathEdit->setText(path);
    });

    // ---- clangd 参数 ----
    auto *clangdArgsRow = new QHBoxLayout;
    auto *clangdArgsLabel = new QLabel(tr("clangd 参数"));
    clangdArgsLabel->setStyleSheet(labelStyle());
    m_clangdArgsEdit = new QLineEdit;
    m_clangdArgsEdit->setPlaceholderText(QStringLiteral("--fallback-style=Google"));
    m_clangdArgsEdit->setStyleSheet(inputStyle());
    m_clangdArgsEdit->setText(cfg.toolClangdArgs());
    clangdArgsRow->addWidget(clangdArgsLabel);
    clangdArgsRow->addStretch();
    clangdArgsRow->addWidget(m_clangdArgsEdit, 1);
    layout->addLayout(clangdArgsRow);

    connect(m_clangdArgsEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit toolSettingChanged("tools.clangd.args", text);
    });

    // ---- Python 路径 ----
    auto *pythonPathRow = new QHBoxLayout;
    auto *pythonPathLabel = new QLabel(tr("Python 路径"));
    pythonPathLabel->setStyleSheet(labelStyle());
    m_pythonPathEdit = new QLineEdit;
    m_pythonPathEdit->setPlaceholderText(tr("留空则从 PATH 查找"));
    m_pythonPathEdit->setStyleSheet(inputStyle());
    m_pythonPathEdit->setText(cfg.toolPythonPath());
    m_pythonBrowseBtn = new QPushButton(tr("浏览…"));
    m_pythonBrowseBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "padding: 4px 12px; border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background: %4; }")
        .arg(ThemeManager::instance().color("input.background").name(),
             ThemeManager::instance().color("input.foreground").name(),
             ThemeManager::instance().color("input.border").name(),
             ThemeManager::instance().color("aiAssistant.actionButtonHoverBackground").name()));
    pythonPathRow->addWidget(pythonPathLabel);
    pythonPathRow->addStretch();
    pythonPathRow->addWidget(m_pythonPathEdit, 1);
    pythonPathRow->addWidget(m_pythonBrowseBtn);
    layout->addLayout(pythonPathRow);

    connect(m_pythonPathEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit toolSettingChanged("tools.python.path", text);
    });
    connect(m_pythonBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("选择 Python 可执行文件"));
        if (!path.isEmpty())
            m_pythonPathEdit->setText(path);
    });

    // ====================================================================
    // Section: 编译器
    // ====================================================================
    layout->addSpacing(12);
    layout->addWidget(createSectionLabel(tr("编译器")));

    // ---- GCC/Clang flags ----
    auto *gxxRow = new QHBoxLayout;
    auto *gxxLabel = new QLabel(tr("GCC/Clang flags"));
    gxxLabel->setStyleSheet(labelStyle());
    m_gxxFlagsEdit = new QLineEdit;
    m_gxxFlagsEdit->setPlaceholderText(tr("以空格分隔"));
    m_gxxFlagsEdit->setStyleSheet(inputStyle());
    m_gxxFlagsEdit->setText(cfg.compilerGxxFlags().join(" "));
    gxxRow->addWidget(gxxLabel);
    gxxRow->addStretch();
    gxxRow->addWidget(m_gxxFlagsEdit, 1);
    layout->addLayout(gxxRow);

    connect(m_gxxFlagsEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit toolSettingChanged("compiler.gxx_flags", text);
    });

    // ---- MSVC flags ----
    auto *msvcRow = new QHBoxLayout;
    auto *msvcLabel = new QLabel(tr("MSVC flags"));
    msvcLabel->setStyleSheet(labelStyle());
    m_msvcFlagsEdit = new QLineEdit;
    m_msvcFlagsEdit->setPlaceholderText(tr("以空格分隔"));
    m_msvcFlagsEdit->setStyleSheet(inputStyle());
    m_msvcFlagsEdit->setText(cfg.compilerMsvcFlags().join(" "));
    msvcRow->addWidget(msvcLabel);
    msvcRow->addStretch();
    msvcRow->addWidget(m_msvcFlagsEdit, 1);
    layout->addLayout(msvcRow);

    connect(m_msvcFlagsEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit toolSettingChanged("compiler.msvc_flags", text);
    });

    // ====================================================================
    // Section: 评测
    // ====================================================================
    layout->addSpacing(12);
    layout->addWidget(createSectionLabel(tr("评测")));

    // ---- 时间限制 ----
    auto *timeLimitRow = new QHBoxLayout;
    auto *timeLimitLabel = new QLabel(tr("时间限制（ms）"));
    timeLimitLabel->setStyleSheet(labelStyle());
    m_judgeTimeLimitSpin = new ClampSpinBox;
    m_judgeTimeLimitSpin->setRange(100, 10000);
    m_judgeTimeLimitSpin->setSingleStep(100);
    m_judgeTimeLimitSpin->setValue(cfg.judgeTimeLimitMs());
    m_judgeTimeLimitSpin->setFixedWidth(100);
    m_judgeTimeLimitSpin->setStyleSheet(inputStyle());
    timeLimitRow->addWidget(timeLimitLabel);
    timeLimitRow->addStretch();
    timeLimitRow->addWidget(m_judgeTimeLimitSpin);
    layout->addLayout(timeLimitRow);

    connect(m_judgeTimeLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit toolSettingChanged("judge.time_limit_ms", val);
    });

    // ---- 内存限制 ----
    auto *memLimitRow = new QHBoxLayout;
    auto *memLimitLabel = new QLabel(tr("内存限制（MB）"));
    memLimitLabel->setStyleSheet(labelStyle());
    m_judgeMemoryLimitSpin = new ClampSpinBox;
    m_judgeMemoryLimitSpin->setRange(16, 1024);
    m_judgeMemoryLimitSpin->setSingleStep(16);
    m_judgeMemoryLimitSpin->setValue(cfg.judgeMemoryLimitKb() / 1024);
    m_judgeMemoryLimitSpin->setFixedWidth(100);
    m_judgeMemoryLimitSpin->setStyleSheet(inputStyle());
    memLimitRow->addWidget(memLimitLabel);
    memLimitRow->addStretch();
    memLimitRow->addWidget(m_judgeMemoryLimitSpin);
    layout->addLayout(memLimitRow);

    connect(m_judgeMemoryLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        emit toolSettingChanged("judge.memory_limit_kb", val * 1024);
    });

    // ====================================================================
    // Section: OpenJudge
    // ====================================================================
    layout->addSpacing(12);
    layout->addWidget(createSectionLabel(tr("OpenJudge")));

    // ---- 服务器 URL ----
    auto *urlRow = new QHBoxLayout;
    auto *urlLabel = new QLabel(tr("服务器 URL"));
    urlLabel->setStyleSheet(labelStyle());
    m_openJudgeUrlEdit = new QLineEdit;
    m_openJudgeUrlEdit->setPlaceholderText(QStringLiteral("https://"));
    m_openJudgeUrlEdit->setStyleSheet(inputStyle());
    m_openJudgeUrlEdit->setText(cfg.openJudgeBaseUrl());
    urlRow->addWidget(urlLabel);
    urlRow->addStretch();
    urlRow->addWidget(m_openJudgeUrlEdit, 1);
    layout->addLayout(urlRow);

    connect(m_openJudgeUrlEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit toolSettingChanged("open_judge.base_url", text);
    });

    // ---- 自动登录 ----
    auto *autoLoginRow = new QHBoxLayout;
    auto *autoLoginLabel = new QLabel(tr("自动登录"));
    autoLoginLabel->setStyleSheet(labelStyle());

    m_openJudgeAutoLoginToggle = new ToggleSwitch;
    m_openJudgeAutoLoginToggle->setChecked(false);

    auto *autoLoginStateLabel = new QLabel(tr("关"));
    autoLoginStateLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
        .arg(ThemeManager::instance().color("tab.inactiveForeground").name()));

    m_openJudgeAutoLoginToggle->onToggled = [this, autoLoginStateLabel](bool checked) {
        autoLoginStateLabel->setText(checked ? tr("开") : tr("关"));
        autoLoginStateLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
            .arg(checked ? ThemeManager::instance().color("badge.background").name()
                         : ThemeManager::instance().color("tab.inactiveForeground").name()));
        emit toolSettingChanged("open_judge.auto_login", checked);
    };

    auto *toggleWidget = new QWidget;
    auto *toggleLayout = new QHBoxLayout(toggleWidget);
    toggleLayout->setContentsMargins(0, 0, 0, 0);
    toggleLayout->setSpacing(6);
    toggleLayout->addWidget(m_openJudgeAutoLoginToggle);
    toggleLayout->addWidget(autoLoginStateLabel);

    autoLoginRow->addWidget(autoLoginLabel);
    autoLoginRow->addStretch();
    autoLoginRow->addWidget(toggleWidget);
    layout->addLayout(autoLoginRow);

    // ---- 用户名 ----
    auto *usernameRow = new QHBoxLayout;
    auto *usernameLabel = new QLabel(tr("用户名"));
    usernameLabel->setStyleSheet(labelStyle());
    m_openJudgeUsernameEdit = new QLineEdit;
    m_openJudgeUsernameEdit->setStyleSheet(inputStyle());
    usernameRow->addWidget(usernameLabel);
    usernameRow->addStretch();
    usernameRow->addWidget(m_openJudgeUsernameEdit, 1);
    layout->addLayout(usernameRow);

    connect(m_openJudgeUsernameEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit toolSettingChanged("open_judge.username", text);
    });

    // ---- 密码 ----
    auto *passwordRow = new QHBoxLayout;
    auto *passwordLabel = new QLabel(tr("密码"));
    passwordLabel->setStyleSheet(labelStyle());
    m_openJudgePasswordEdit = new QLineEdit;
    m_openJudgePasswordEdit->setEchoMode(QLineEdit::Password);
    m_openJudgePasswordEdit->setStyleSheet(inputStyle());
    m_openJudgePasswordToggleBtn = new QPushButton(tr("显示"));
    m_openJudgePasswordToggleBtn->setFixedSize(50, 28);
    m_openJudgePasswordToggleBtn->setCursor(Qt::PointingHandCursor);
    m_openJudgePasswordToggleBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background: %4; }")
        .arg(ThemeManager::instance().color("input.background").name(),
             ThemeManager::instance().color("input.foreground").name(),
             ThemeManager::instance().color("input.border").name(),
             ThemeManager::instance().color("aiAssistant.actionButtonHoverBackground").name()));
    passwordRow->addWidget(passwordLabel);
    passwordRow->addStretch();
    passwordRow->addWidget(m_openJudgePasswordEdit, 1);
    passwordRow->addWidget(m_openJudgePasswordToggleBtn);
    layout->addLayout(passwordRow);

    connect(m_openJudgePasswordEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit toolSettingChanged("open_judge.password", text);
    });
    connect(m_openJudgePasswordToggleBtn, &QPushButton::clicked, this, [this]() {
        bool hidden = (m_openJudgePasswordEdit->echoMode() == QLineEdit::Password);
        m_openJudgePasswordEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        m_openJudgePasswordToggleBtn->setText(hidden ? tr("隐藏") : tr("显示"));
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
    if (m_markdownIndentWidthSpin) {
        m_markdownIndentWidthSpin->setValue(sm.value("editor.markdown_indent_width", cfg.editorMarkdownIndentWidth()).toInt());
    }
    if (m_autoSaveToggle) {
        m_autoSaveToggle->setChecked(sm.value("auto_save.enabled", cfg.autoSaveEnabled()).toBool());
    }
    if (m_autoSaveIntervalSpin) {
        m_autoSaveIntervalSpin->setValue(sm.value("auto_save.interval_ms", cfg.autoSaveIntervalMs()).toInt() / 1000);
    }

    // Appearance page
    if (m_fileTreeItemHeightSpin) {
        m_fileTreeItemHeightSpin->setValue(sm.value("editor.file_tree_item_height", cfg.editorFileTreeItemHeight()).toInt());
    }
    if (m_equalWidthTabToggle) {
        m_equalWidthTabToggle->setChecked(sm.value("editor.equal_width_tab", false).toBool());
    }

    // Refresh color controls (appearance page)
    for (const auto &cc : m_colorControls) {
        QString hex = sm.value(cc.configKey, cc.defaultColor.name()).toString();
        auto &ctm = ThemeManager::instance();
        cc.btn->setStyleSheet(
            QStringLiteral(
                "QPushButton { background-color: %1; border: 1px solid %2; border-radius: 4px; }"
                "QPushButton:hover { border-color: %3; }"
            ).arg(hex, ctm.color("input.border").name(), ctm.color("badge.background").name()));
        cc.preview->setText(hex);
    }

    // Output panel (now in editor page)
    if (m_outputFontFamilyCombo) {
        QString outFamily = sm.value("output_panel.font.family", "").toString();
        if (!outFamily.isEmpty()) {
            int idx = m_outputFontFamilyCombo->findText(outFamily);
            if (idx >= 0) m_outputFontFamilyCombo->setCurrentIndex(idx);
        }
    }
    if (m_outputFontSizeSpin) {
        m_outputFontSizeSpin->setValue(sm.value("output_panel.font.size", cfg.outputPanelFontSize()).toInt());
    }
    if (m_outputMaxBlocksSpin) {
        m_outputMaxBlocksSpin->setValue(sm.value("output_panel.max_blocks", cfg.outputPanelMaxBlocks()).toInt());
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

    // Tools page
    if (m_clangdPathEdit)
        m_clangdPathEdit->setText(sm.value("tools.clangd.path", cfg.toolClangdPath()).toString());
    if (m_clangdArgsEdit)
        m_clangdArgsEdit->setText(sm.value("tools.clangd.args", cfg.toolClangdArgs()).toString());
    if (m_pythonPathEdit)
        m_pythonPathEdit->setText(sm.value("tools.python.path", cfg.toolPythonPath()).toString());
    if (m_gxxFlagsEdit)
        m_gxxFlagsEdit->setText(sm.value("compiler.gxx_flags", cfg.compilerGxxFlags().join(" ")).toString());
    if (m_msvcFlagsEdit)
        m_msvcFlagsEdit->setText(sm.value("compiler.msvc_flags", cfg.compilerMsvcFlags().join(" ")).toString());
    if (m_judgeTimeLimitSpin)
        m_judgeTimeLimitSpin->setValue(sm.value("judge.time_limit_ms", cfg.judgeTimeLimitMs()).toInt());
    if (m_judgeMemoryLimitSpin)
        m_judgeMemoryLimitSpin->setValue(sm.value("judge.memory_limit_kb", cfg.judgeMemoryLimitKb()).toInt() / 1024);
    if (m_openJudgeUrlEdit)
        m_openJudgeUrlEdit->setText(sm.value("open_judge.base_url", cfg.openJudgeBaseUrl()).toString());
    if (m_openJudgeAutoLoginToggle)
        m_openJudgeAutoLoginToggle->setChecked(sm.openJudgeAutoLogin());
    if (m_openJudgeUsernameEdit || m_openJudgePasswordEdit) {
        auto creds = sm.openJudgeCredentials();
        if (m_openJudgeUsernameEdit)
            m_openJudgeUsernameEdit->setText(creds.first);
        if (m_openJudgePasswordEdit)
            m_openJudgePasswordEdit->setText(creds.second);
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

bool SettingsPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_aiSystemPromptEdit && event->type() == QEvent::FocusOut) {
        // 焦点离开时立即 emit 最终值
        emit aiSettingChanged("ai.system_prompt", m_aiSystemPromptEdit->toPlainText());
        m_aiPromptDebounceTimer->stop();
    }
    if (event->type() == QEvent::Wheel && qobject_cast<QComboBox*>(watched))
        return true;
    return QWidget::eventFilter(watched, event);
}

#include "settingspanel.moc"
