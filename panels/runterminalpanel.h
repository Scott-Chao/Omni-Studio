#ifndef RUNTERMINALPANEL_H
#define RUNTERMINALPANEL_H

#include <QWidget>

class QPushButton;
class TerminalView;

class RunTerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RunTerminalPanel(QWidget *parent = nullptr);

    void appendOutput(const QString &text, bool isStderr);
    void clearOutput();
    void setStatus(const QString &status, bool isError = false);
    void setRunning(bool running);
    void setInputEnabled(bool enabled);
    void enableTextSelection(bool enabled);
    void reloadShortcuts();
    QWidget *focusTarget() const;

signals:
    void stopRequested();
    void sendInput(const QString &text);
    void sendRawInput(const QString &text);

private:
    TerminalView *m_view = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;
    bool m_running = false;
    bool m_acceptingInput = false;

    void refreshStyle();
};

#endif // RUNTERMINALPANEL_H
