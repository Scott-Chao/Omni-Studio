#ifndef TABBUTTONGROUP_H
#define TABBUTTONGROUP_H

#include <QObject>
#include <QHash>

class QPushButton;
class QStackedWidget;

class TabButtonGroup : public QObject
{
    Q_OBJECT

public:
    using StyleProvider = QString(*)(int index, bool active);

    TabButtonGroup(QStackedWidget *stack, QObject *parent = nullptr);

    void addTab(QPushButton *button, int index);
    void setCurrentIndex(int index);
    int currentIndex() const { return m_currentIndex; }

    // If set, called for each button on every switch to determine per-tab style.
    // If not set, buttons are not styled automatically.
    void setStyleProvider(StyleProvider provider) { m_provider = provider; }

    void refreshStyles();
    QStackedWidget *stackWidget() const { return m_stack; }

signals:
    void currentChanged(int index);

private:
    void onButtonClicked(int index);

    QStackedWidget *m_stack;
    QHash<QPushButton*, int> m_map;
    int m_currentIndex = 0;
    StyleProvider m_provider = nullptr;
};

#endif // TABBUTTONGROUP_H
