#ifndef RIGHTPANELCONTAINER_H
#define RIGHTPANELCONTAINER_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QIcon>
#include <QVector>

class HistoryPanel;
class OutlinePanel;
class TagPanel;
class BacklinksPanel;
class SettingsManager;

class RightPanelContainer : public QWidget
{
    Q_OBJECT
public:
    explicit RightPanelContainer(SettingsManager *settings, QWidget *parent = nullptr);

    HistoryPanel *historyPanel() const { return m_historyPanel; }
    OutlinePanel *outlinePanel() const { return m_outlinePanel; }
    TagPanel *tagPanel() const { return m_tagPanel; }
    BacklinksPanel *backlinksPanel() const { return m_backlinksPanel; }

    void setActivePanel(int index);
    int currentPanel() const { return m_currentPanel; }

signals:
    void fileClicked(const QString &filePath);
    void tagClicked(const QString &tag);
    void headingClicked(int lineNumber, const QString &headingText);
    void activePanelChanged(int index);

private:
    QPushButton *createTabButton(const QString &text, const QIcon &icon, int index);
    void updateTabStyles(int activeIndex);
    void refreshStyle();

    QWidget *m_tabBar;
    QVector<QPushButton*> m_tabButtons;
    QStackedWidget *m_stack;
    int m_currentPanel = 0;

    HistoryPanel *m_historyPanel;
    OutlinePanel *m_outlinePanel;
    TagPanel *m_tagPanel;
    BacklinksPanel *m_backlinksPanel;
};

#endif // RIGHTPANELCONTAINER_H
