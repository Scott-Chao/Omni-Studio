#ifndef ACTIONBAR_H
#define ACTIONBAR_H

#include <QWidget>
#include <QVector>
#include "prompttemplates.h"

class FlowLayout;
class QFrame;

class ActionBar : public QWidget
{
    Q_OBJECT

public:
    explicit ActionBar(QWidget *parent = nullptr);

    void setActions(const QVector<AiAction> &actions);
    void clearActions();

signals:
    void actionTriggered(AiAction action);

private:
    void rebuildButtons();
    void refreshStyle();

    FlowLayout *m_layout;
    QFrame *m_separator;
    QVector<AiAction> m_currentActions;
};

#endif // ACTIONBAR_H
