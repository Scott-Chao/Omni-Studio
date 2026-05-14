#include "activitybar.h"
#include <QFont>

ActivityBar::ActivityBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(48);
    setStyleSheet("background-color: #333337;");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(0);

    m_searchBtn    = createButton(QStringLiteral("\xF0\x9F\x94\x8D"), tr("搜索 (Ctrl+Shift+F)"));
    m_settingsBtn  = createButton(QStringLiteral("\xE2\x9A\x99"),    tr("设置 (Ctrl+,)"));
    m_exportPdfBtn = createButton(QStringLiteral("\xF0\x9F\x93\x84"), tr("导出 PDF (Ctrl+E)"));
    m_judgeBtn     = createButton(QStringLiteral("\xE2\x9C\x93"),    tr("评测 (Ctrl+Shift+J)"));

    layout->addWidget(m_searchBtn);
    layout->addStretch(1);
    layout->addWidget(m_settingsBtn);
    layout->addWidget(m_exportPdfBtn);
    layout->addWidget(m_judgeBtn);

    connect(m_searchBtn,    &QPushButton::clicked, this, &ActivityBar::searchClicked);
    connect(m_settingsBtn,  &QPushButton::clicked, this, &ActivityBar::settingsClicked);
    connect(m_exportPdfBtn, &QPushButton::clicked, this, &ActivityBar::exportPdfClicked);
    connect(m_judgeBtn,     &QPushButton::clicked, this, &ActivityBar::judgeClicked);
}

QPushButton *ActivityBar::createButton(const QString &text, const QString &tooltip)
{
    QPushButton *btn = new QPushButton(text, this);
    btn->setFixedSize(48, 48);
    btn->setToolTip(tooltip);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    QFont f = btn->font();
    f.setPointSize(16);
    btn->setFont(f);
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
            "  color: #ffffff;"
            "}"
        );
    }
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-left: 2px solid transparent;"
        "  color: #888888;"
        "}"
        "QPushButton:hover {"
        "  background: #2d2d2d;"
        "  color: #cccccc;"
        "}"
    );
}

void ActivityBar::updateButtonStyle(QPushButton *btn, bool active)
{
    btn->setStyleSheet(buttonStyleSheet(active));
}

void ActivityBar::setSearchActive(bool active) { updateButtonStyle(m_searchBtn, active); }
void ActivityBar::setJudgeActive(bool active)  { updateButtonStyle(m_judgeBtn, active); }

void ActivityBar::setExportPdfVisible(bool visible)
{
    m_exportPdfBtn->setVisible(visible);
}
