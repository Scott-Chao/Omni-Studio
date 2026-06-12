#ifndef TERMINALPANEL_H
#define TERMINALPANEL_H

#include <QWidget>
#include <functional>

class QTabWidget;
class QPushButton;
class TerminalView;

class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget *parent = nullptr);
    ~TerminalPanel() override;

    void setWorkingDirectoryProvider(std::function<QString()> provider);
    void ensureTerminal();
    void openTerminal();
    int terminalCount() const;

signals:
    void closeRequested();

private:
    QTabWidget *m_tabs = nullptr;
    QPushButton *m_newTerminalBtn = nullptr;
    QPushButton *m_closePanelBtn = nullptr;
    std::function<QString()> m_workingDirectoryProvider;
    int m_nextTerminalId = 1;

    TerminalView *createTerminalView(const QString &workingDirectory);
    QString workingDirectory() const;
    void closeTerminal(int index);
    void refreshStyle();
};

#endif // TERMINALPANEL_H
