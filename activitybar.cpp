#include "activitybar.h"
#include <QFont>
#include <QIcon>

ActivityBar::ActivityBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(48);
    setStyleSheet("background-color: #333337;");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(0);

    m_searchBtn    = createButton(QIcon(":/icons/search"),    tr("搜索 (Ctrl+Shift+F)"));
    m_aiBtn        = createButton(QIcon(":/icons/ai"),        tr("AI 助手 (Ctrl+Shift+A)"));
    m_settingsBtn  = createButton(QIcon(":/icons/settings"),  tr("设置 (Ctrl+,)"));
    m_exportPdfBtn = createButton(QIcon(":/icons/pdf"),       tr("导出 PDF (Ctrl+E)"));
    m_exportPdfBtn->setVisible(false);
    m_judgeBtn     = createButton(QIcon(":/icons/judge"),     tr("评测 (Ctrl+Shift+J)"));

    layout->addWidget(m_searchBtn);
    layout->addWidget(m_aiBtn);
    layout->addStretch(1);
    layout->addWidget(m_settingsBtn);
    layout->addWidget(m_exportPdfBtn);
    layout->addWidget(m_judgeBtn);

    connect(m_searchBtn,    &QPushButton::clicked, this, &ActivityBar::searchClicked);
    connect(m_aiBtn,        &QPushButton::clicked, this, &ActivityBar::aiClicked);
    connect(m_settingsBtn,  &QPushButton::clicked, this, &ActivityBar::settingsClicked);
    connect(m_exportPdfBtn, &QPushButton::clicked, this, &ActivityBar::exportPdfClicked);
    connect(m_judgeBtn,     &QPushButton::clicked, this, &ActivityBar::judgeClicked);
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

QString ActivityBar::buttonStyleSheet(bool active) const
{
    if (active) {
        return QStringLiteral(
            "QPushButton {"
            "  background: #2d2d2d;"
            "  border: none;"
            "  border-left: 2px solid #0078D4;"
            "}"
        );
    }
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-left: 2px solid transparent;"
        "}"
        "QPushButton:hover {"
        "  background: #2d2d2d;"
        "}"
    );
}

void ActivityBar::updateButtonStyle(QPushButton *btn, bool active)
{
    btn->setStyleSheet(buttonStyleSheet(active));
}

void ActivityBar::setSearchActive(bool active) { updateButtonStyle(m_searchBtn, active); }
void ActivityBar::setAiActive(bool active)    { updateButtonStyle(m_aiBtn, active); }
void ActivityBar::setJudgeActive(bool active)  { updateButtonStyle(m_judgeBtn, active); }

void ActivityBar::setExportPdfVisible(bool visible)
{
    m_exportPdfBtn->setVisible(visible);
}
