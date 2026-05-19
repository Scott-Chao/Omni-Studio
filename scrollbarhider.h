#ifndef SCROLLBARHIDER_H
#define SCROLLBARHIDER_H

#include <QObject>
#include <QSet>
#include <QPointer>

class QAbstractScrollArea;
class QTimer;

class ScrollbarHider : public QObject
{
    Q_OBJECT
public:
    explicit ScrollbarHider(QObject *parent = nullptr);

    void manage(QAbstractScrollArea *area);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onHideTimeout();

private:
    void attach(QObject *target, QAbstractScrollArea *area);
    void setScrollbarVisible(QAbstractScrollArea *area, bool visible);
    QString makeScrollbarQss(bool visible) const;

    // Maps watched objects (viewport, scrollbar) → area
    QHash<QObject*, QPointer<QAbstractScrollArea>> m_watched;

    // One hide timer per area
    QHash<QAbstractScrollArea*, QTimer*> m_areaTimers;

    QSet<QAbstractScrollArea*> m_managed;
};

#endif // SCROLLBARHIDER_H
