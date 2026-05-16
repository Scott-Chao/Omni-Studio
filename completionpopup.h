#ifndef COMPLETIONPOPUP_H
#define COMPLETIONPOPUP_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QStyledItemDelegate>

#include "completionprovider.h"

class CompletionItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

class CompletionPopup : public QWidget
{
    Q_OBJECT

public:
    explicit CompletionPopup(QWidget *parent = nullptr);

    void showItems(const QList<CompletionItem> &items);
    void selectNext();
    void selectPrevious();
    CompletionItem selectedItem() const;
    bool isActive() const { return isVisible() && m_listWidget->count() > 0; }

signals:
    void itemSelected(const CompletionItem &item);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QListWidget *m_listWidget;
    QLabel *m_hintLabel;
    QList<CompletionItem> m_items;

    QIcon iconForType(const QString &type) const;
};

#endif // COMPLETIONPOPUP_H
