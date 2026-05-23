#ifndef AIHISTORYLISTWIDGET_H
#define AIHISTORYLISTWIDGET_H

#include <QWidget>
#include <QList>
#include "aiconversation.h"

class QLineEdit;
class QPushButton;
class QListWidget;
class QListWidgetItem;

class AiHistoryListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AiHistoryListWidget(QWidget *parent = nullptr);

    void setConversations(const QList<AiConversation> &convs);
    void setActiveConversationId(const QString &id);
    QString activeConversationId() const { return m_activeConvId; }

signals:
    void conversationSelected(const QString &convId);
    void renameRequested(const QString &convId);
    void deleteRequested(const QString &convId);
    void exportRequested(const QString &convId);

private slots:
    void onSearchChanged(const QString &text);
    void onItemClicked(QListWidgetItem *item);
    void onContextMenu(const QPoint &pos);
    void refreshStyle();

private:
    void rebuildList();
    QString dateGroupLabel(const QDateTime &dt) const;
    bool isSameDay(const QDateTime &a, const QDateTime &b) const;

    QLineEdit *m_searchEdit;
    QPushButton *m_clearSearchBtn;
    QListWidget *m_listWidget;

    QList<AiConversation> m_allConversations;
    QString m_activeConvId;
};

#endif // AIHISTORYLISTWIDGET_H
