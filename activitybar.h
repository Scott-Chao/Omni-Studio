#ifndef ACTIVITYBAR_H
#define ACTIVITYBAR_H

#include <QWidget>
#include <QPushButton>
#include <QIcon>
#include <QVBoxLayout>

class ActivityBar : public QWidget
{
    Q_OBJECT
public:
    explicit ActivityBar(QWidget *parent = nullptr);

    void setSearchActive(bool active);
    void setJudgeActive(bool active);
    void setAiActive(bool active);
    void setSettingsActive(bool active);
    void setExplorerActive(bool active);
    void setExportPdfVisible(bool visible);

signals:
    void explorerClicked();
    void searchClicked();
    void settingsClicked();
    void exportPdfClicked();
    void judgeClicked();
    void aiClicked();

private:
    QPushButton *createButton(const QIcon &icon, const QString &tooltip);
    void updateButtonStyle(QPushButton *btn, bool active);
    QString buttonStyleSheet(bool active) const;
    void refreshStyle();

    QPushButton *m_explorerBtn;
    QPushButton *m_searchBtn;
    QPushButton *m_aiBtn;
    QPushButton *m_settingsBtn;
    QPushButton *m_exportPdfBtn;
    QPushButton *m_judgeBtn;

    bool m_explorerActive = false;
    bool m_searchActive = false;
    bool m_aiActive = false;
    bool m_settingsActive = false;
    bool m_judgeActive = false;
};

#endif // ACTIVITYBAR_H
