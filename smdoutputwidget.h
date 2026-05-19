#ifndef SMDOUTPUTWIDGET_H
#define SMDOUTPUTWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>

class SmdOutputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SmdOutputWidget(QWidget *parent = nullptr);

    void setOutput(const QString &text);
    void appendText(const QString &text, bool isStderr = false);
    void clearOutput();
    void clearSelection();
    void scrollToTop();
    QString outputText() const;
    bool hasOutput() const;

    static constexpr int kMaxOutputLines = 1000;
    static constexpr int kMaxVisibleLines = 15;

private slots:
    void updateHeight();

private:
    QPlainTextEdit *m_outputEdit = nullptr;
    int m_hiddenLineCount = 0;
};

#endif // SMDOUTPUTWIDGET_H
