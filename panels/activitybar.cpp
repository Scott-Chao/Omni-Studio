#include "activitybar.h"
#include "core/thememanager.h"
#include <QFont>
#include <QIcon>
#include <QPainter>

namespace {

// Convert "Ctrl+B" to native display text ("⌘B" on macOS, "Ctrl+B" on Win).
// Qt 6's QKeySequence::NativeText handles the platform mapping.
static QString nativeShortcutText(const char *shortcut)
{
    return QKeySequence(QString::fromLatin1(shortcut))
        .toString(QKeySequence::NativeText);
}

QIcon coloredSvgIcon(const QString &svgPath, const QColor &color, int size)
{
    // Create a solid-colored pixmap, then apply the SVG shape as a mask
    // via DestinationIn composition. This avoids platform differences in
    // QImage format conversion (macOS CoreGraphics vs Windows GDI).
    QIcon src(svgPath);

    QPixmap result(size, size);
    result.fill(color);

    QPainter p(&result);
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    src.paint(&p, 0, 0, size, size);
    p.end();
    return QIcon(result);
}
} // anonymous namespace

ActivityBar::ActivityBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(48);
    setAutoFillBackground(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(0);

    m_explorerBtn = createButton(QIcon(":/icons/file"),
        tr("文件浏览 (%1)").arg(nativeShortcutText("Ctrl+B")));
    m_searchBtn    = createButton(QIcon(":/icons/search"),
        tr("搜索 (%1)").arg(nativeShortcutText("Ctrl+Shift+F")));
    m_aiBtn        = createButton(QIcon(":/icons/ai"),
        tr("AI 助手 (%1)").arg(nativeShortcutText("Ctrl+Shift+A")));
    m_settingsBtn  = createButton(QIcon(":/icons/settings"),
        tr("设置 (%1)").arg(nativeShortcutText("Ctrl+,")));
    m_exportPdfBtn = createButton(QIcon(":/icons/pdf"),
        tr("导出 PDF (%1)").arg(nativeShortcutText("Ctrl+E")));
    m_exportPdfBtn->setVisible(false);
    m_judgeBtn     = createButton(QIcon(":/icons/judge"),
        tr("评测 (%1)").arg(nativeShortcutText("Ctrl+Shift+J")));

    layout->addWidget(m_explorerBtn);
    layout->addWidget(m_searchBtn);
    layout->addWidget(m_aiBtn);
    layout->addWidget(m_judgeBtn);
    layout->addWidget(m_exportPdfBtn);
    layout->addStretch(1);
    layout->addWidget(m_settingsBtn);

    connect(m_explorerBtn, &QPushButton::clicked, this, &ActivityBar::explorerClicked);
    connect(m_searchBtn,    &QPushButton::clicked, this, &ActivityBar::searchClicked);
    connect(m_aiBtn,        &QPushButton::clicked, this, &ActivityBar::aiClicked);
    connect(m_settingsBtn,  &QPushButton::clicked, this, &ActivityBar::settingsClicked);
    connect(m_exportPdfBtn, &QPushButton::clicked, this, &ActivityBar::exportPdfClicked);
    connect(m_judgeBtn,     &QPushButton::clicked, this, &ActivityBar::judgeClicked);

    ThemeManager::watchTheme(this, &ActivityBar::refreshStyle);
    refreshStyle();
}

void ActivityBar::refreshStyle()
{
    auto &tm = ThemeManager::instance();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, tm.color("activityBar.background"));
    setPalette(pal);

    QColor fg = tm.color("activityBar.foreground");
    const int iconSize = 22;
    m_explorerBtn->setIcon(coloredSvgIcon(":/icons/file", fg, iconSize));
    m_searchBtn->setIcon(coloredSvgIcon(":/icons/search", fg, iconSize));
    m_aiBtn->setIcon(coloredSvgIcon(":/icons/ai", fg, iconSize));
    m_settingsBtn->setIcon(coloredSvgIcon(":/icons/settings", fg, iconSize));
    m_exportPdfBtn->setIcon(coloredSvgIcon(":/icons/pdf", fg, iconSize));
    m_judgeBtn->setIcon(coloredSvgIcon(":/icons/judge", fg, iconSize));

    updateButtonStyle(m_explorerBtn, m_explorerActive);
    updateButtonStyle(m_searchBtn, m_searchActive);
    updateButtonStyle(m_aiBtn, m_aiActive);
    updateButtonStyle(m_settingsBtn, m_settingsActive);
    updateButtonStyle(m_exportPdfBtn, false);
    updateButtonStyle(m_judgeBtn, m_judgeActive);
}

QPushButton *ActivityBar::createButton(const QIcon &icon, const QString &tooltip)
{
    QPushButton *btn = new QPushButton(this);
    btn->setFixedSize(48, 48);
    btn->setIcon(icon);
    btn->setIconSize(QSize(22, 22));
    btn->setToolTip(tooltip);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(buttonStyleSheet(false));
    return btn;
}

namespace {
QString toStyleColor(const QColor &c)
{
    if (c.alpha() < 255)
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alphaF(), 0, 'f', 2);
    return c.name();
}
}

QString ActivityBar::buttonStyleSheet(bool active) const
{
    auto &tm = ThemeManager::instance();
    QString hoverBg = toStyleColor(tm.color("activityBar.hoverBackground"));
    QString borderColor = toStyleColor(tm.color("activityBar.activeBorder"));

    if (active) {
        return QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  border: none;"
            "  border-left: 2px solid %2;"
            "}"
        ).arg(hoverBg, borderColor);
    }
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-left: 2px solid transparent;"
        "}"
        "QPushButton:hover {"
        "  background: %1;"
        "}"
    ).arg(hoverBg);
}

void ActivityBar::updateButtonStyle(QPushButton *btn, bool active)
{
    btn->setStyleSheet(buttonStyleSheet(active));
}

void ActivityBar::setExplorerActive(bool active) { m_explorerActive = active; updateButtonStyle(m_explorerBtn, active); }
void ActivityBar::setSearchActive(bool active) { m_searchActive = active; updateButtonStyle(m_searchBtn, active); }
void ActivityBar::setAiActive(bool active)    { m_aiActive = active; updateButtonStyle(m_aiBtn, active); }
void ActivityBar::setSettingsActive(bool active) { m_settingsActive = active; updateButtonStyle(m_settingsBtn, active); }
void ActivityBar::setJudgeActive(bool active)  { m_judgeActive = active; updateButtonStyle(m_judgeBtn, active); }

void ActivityBar::setExportPdfVisible(bool visible)
{
    m_exportPdfBtn->setVisible(visible);
}
