#ifndef TERMINALPANEL_H
#define TERMINALPANEL_H

#include <QWidget>
#include <functional>

class QTabWidget;
class QPushButton;
class TerminalView;
class RunTerminalPanel;

class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget *parent = nullptr);
    ~TerminalPanel() override;

    void setWorkingDirectoryProvider(std::function<QString()> provider);
    void ensureTerminal();
    void openTerminal();
    void openCommandTerminal(const QString &title, const QString &command,
                             const QString &workingDirectory, const QString &reuseKey = QString());
    int terminalCount() const;
    RunTerminalPanel *runTerminal() const { return m_runTerminal; }

private:
    QTabWidget *m_tabs = nullptr;
    RunTerminalPanel *m_runTerminal = nullptr;
    std::function<QString()> m_workingDirectoryProvider;
    int m_nextTerminalId = 1;

    TerminalView *createTerminalView(const QString &workingDirectory);
    QString workingDirectory() const;
    void closeTerminal(int index);
    void refreshStyle();
};

#endif // TERMINALPANEL_H
