#ifndef ERRORLISTPANEL_H
#define ERRORLISTPANEL_H

#include <QWidget>
#include <QVector>
#include "errorjournal.h"

class QComboBox;
class QLineEdit;
class QVBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;

class ErrorListItem : public QWidget
{
    Q_OBJECT
public:
    explicit ErrorListItem(const ErrorRecord &record, QWidget *parent = nullptr);
    QString recordId() const { return m_record.id; }

signals:
    void clicked(const QString &recordId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;

private:
    ErrorRecord m_record;
    bool m_hovered = false;
};

class ErrorListPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ErrorListPanel(QWidget *parent = nullptr);

    void loadRecords();
    void setRecords(const QVector<ErrorRecord> &records);

signals:
    void errorClicked(const QString &recordId);
    void deleteAllRequested();
    void deleteRecordRequested(const QString &recordId);

private slots:
    void onFilterChanged();
    void onSearchTextChanged(const QString &text);

private:
    void rebuildList();
    QVector<ErrorRecord> filteredRecords() const;

    QComboBox *m_statusFilter;
    QLineEdit *m_searchEdit;
    QScrollArea *m_scrollArea;
    QWidget *m_listContainer;
    QVBoxLayout *m_listLayout;
    QLabel *m_countLabel;
    QPushButton *m_deleteAllBtn;

    QVector<ErrorRecord> m_allRecords;
};

#endif // ERRORLISTPANEL_H
