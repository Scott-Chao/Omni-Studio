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
class ErrorDetailWidget;

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

// ── Detail expansion widget ──────────────────────────────────────

class ErrorDetailWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ErrorDetailWidget(const ErrorRecord &record, QWidget *parent = nullptr);

    void setAnalysis(const QString &analysis);
    QString recordId() const { return m_record.id; }
    void setReviewed(bool reviewed);

signals:
    void reanalyzeClicked(const QString &recordId);
    void deleteClicked(const QString &recordId);
    void markReviewed(const QString &recordId);

private:
    ErrorRecord m_record;
    QLabel *m_aiAnalysisLabel;
    QPushButton *m_reanalyzeBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_reviewBtn;
};

// ── ErrorListPanel ───────────────────────────────────────────────

class ErrorListPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ErrorListPanel(QWidget *parent = nullptr);

    void loadRecords();
    void setRecords(const QVector<ErrorRecord> &records);

    // Update a specific record's detail display (called when AI analysis arrives)
    void updateAnalysis(const QString &recordId);

signals:
    void errorClicked(const QString &recordId);
    void deleteAllRequested();
    void deleteRecordRequested(const QString &recordId);
    void reanalyzeRequested(const QString &recordId);

private slots:
    void onFilterChanged();
    void onSearchTextChanged(const QString &text);
    void onItemClicked(const QString &recordId);
    void onReanalyzeClicked(const QString &recordId);

private:
    void rebuildList();
    QVector<ErrorRecord> filteredRecords() const;
    ErrorDetailWidget *findDetail(const QString &recordId) const;
    void expandItem(const QString &recordId);
    void collapseItem(const QString &recordId);

    QComboBox *m_statusFilter;
    QLineEdit *m_searchEdit;
    QScrollArea *m_scrollArea;
    QWidget *m_listContainer;
    QVBoxLayout *m_listLayout;
    QLabel *m_countLabel;
    QPushButton *m_deleteAllBtn;

    QVector<ErrorRecord> m_allRecords;
    QString m_expandedId;  // currently expanded detail record ID, empty if none
};

#endif // ERRORLISTPANEL_H
