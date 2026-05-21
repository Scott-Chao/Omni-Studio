#ifndef OUTPUTPANEL_H
#define OUTPUTPANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QStringList>

class OutputPanel : public QWidget
{
    Q_OBJECT

public:
    explicit OutputPanel(QWidget *parent = nullptr);

    void appendOutput(const QString &text, bool isStderr);
    void clearOutput();
    void setStatus(const QString &status, bool isError = false);
    void setRunning(bool running);
    void enableTextSelection(bool enabled);

    void setOutputFont(const QFont &font);
    void setMaxBlocks(int max);

    void reloadShortcuts();

signals:
    void stopRequested();
    void sendInput(const QString &text);
    void sendRawInput(const QString &text);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QPlainTextEdit *m_outputEdit;
    QLabel *m_statusLabel;
    QPushButton *m_stopBtn;
    QPushButton *m_clearBtn;

    void refreshStyle();
    void pasteToInput();
    void sendNextPasteLine();

    bool m_acceptingInput = false;
    QString m_inputBuffer;

    // Configurable shortcuts
    QKeySequence m_stopShortcut;
    QKeySequence m_pasteShortcut;
    QStringList m_pasteQueue;
    bool m_pasteEndsWithNewline = false;
    QTimer *m_pasteTimer = nullptr;
};

#endif // OUTPUTPANEL_H
