#ifndef ACTIVITYBAR_H
#define ACTIVITYBAR_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>

class ActivityBar : public QWidget
{
    Q_OBJECT
public:
    explicit ActivityBar(QWidget *parent = nullptr);

    void setSearchActive(bool active);
    void setHistoryActive(bool active);
    void setOutlineActive(bool active);
    void setTagsActive(bool active);
    void setBacklinksActive(bool active);
    void setJudgeActive(bool active);
    void setExportPdfVisible(bool visible);

signals:
    void searchClicked();
    void historyClicked();
    void outlineClicked();
    void tagsClicked();
    void backlinksClicked();
    void settingsClicked();
    void exportPdfClicked();
    void judgeClicked();

private:
    QPushButton *createButton(const QString &text, const QString &tooltip);
    void updateButtonStyle(QPushButton *btn, bool active);
    QString buttonStyleSheet(bool active) const;

    QPushButton *m_searchBtn;
    QPushButton *m_historyBtn;
    QPushButton *m_outlineBtn;
    QPushButton *m_tagsBtn;
    QPushButton *m_backlinksBtn;
    QPushButton *m_settingsBtn;
    QPushButton *m_exportPdfBtn;
    QPushButton *m_judgeBtn;
};

#endif // ACTIVITYBAR_H
