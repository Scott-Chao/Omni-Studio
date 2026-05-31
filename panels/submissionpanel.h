#ifndef SUBMISSIONPANEL_H
#define SUBMISSIONPANEL_H

#include <QWidget>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

#include "judge/crawler.h"

class SubmitResultPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SubmitResultPanel(QWidget *parent = nullptr);
    void showResult(const SubmissionResult &result);

signals:
    void hideRequested();

private:
    QLabel *m_statusLabel;
    QLabel *m_detailLabel;
    QPlainTextEdit *m_ceEdit;
    QPushButton *m_hideBtn;
    void setupUi();
    void refreshStyle();
};

#endif // SUBMISSIONPANEL_H
