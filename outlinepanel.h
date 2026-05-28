#ifndef OUTLINEPANEL_H
#define OUTLINEPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QVector>

#include "outlineutils.h"

class OutlinePanel : public QWidget
{
    Q_OBJECT
public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    void showHeadings(const QVector<HeadingItem> &headings);
    void clear();
    void refreshStyle();

signals:
    void headingClicked(int lineNumber, const QString &headingText);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    QListWidget *m_listWidget;
    QVector<HeadingItem> m_headings;
};

#endif // OUTLINEPANEL_H
