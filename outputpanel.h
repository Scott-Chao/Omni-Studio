#ifndef OUTPUTPANEL_H
#define OUTPUTPANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>

class OutputPanel : public QWidget
{
    Q_OBJECT

public:
    explicit OutputPanel(QWidget *parent = nullptr);

    void appendOutput(const QString &text, bool isStderr);
    void clearOutput();
    void setStatus(const QString &status, bool isError = false);
    void setRunning(bool running);

signals:
    void stopRequested();

private:
    QPlainTextEdit *m_outputEdit;
    QLabel *m_statusLabel;
    QPushButton *m_stopBtn;
    QPushButton *m_clearBtn;
};

#endif // OUTPUTPANEL_H
