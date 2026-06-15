#ifndef BOTTOMPANEL_H
#define BOTTOMPANEL_H

#include <QWidget>
#include <QStackedWidget>
#include <QList>
#include <QLabel>
#include <functional>

#include "smd/smddiagnostic.h"
#include "widgets/tabbuttongroup.h"

class QPushButton;
class DiagnosticSection;
class CodeEditor;
class QScrollArea;
class TerminalPanel;
class RunTerminalPanel;
class SubmitResultPanel;
struct SubmissionResult;

class BottomPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BottomPanel(QWidget *parent = nullptr);

    enum Tab { DiagnosticsTab, TerminalTab, JudgeTab };

    TerminalPanel *terminalPanel() const { return m_terminalPanel; }
    RunTerminalPanel *runTerminal() const;
    SubmitResultPanel *submitResultPanel() const { return m_submitResultPanel; }

    Tab currentTab() const { return static_cast<Tab>(m_tabGroup->currentIndex()); }
    void showDiagnosticsTab() { m_tabGroup->setCurrentIndex(DiagnosticsTab); }
    void showTerminalTab();
    void showJudgeTab() { m_tabGroup->setCurrentIndex(JudgeTab); }
    void showSubmissionResult(const SubmissionResult &result);
    void setWorkingDirectoryProvider(std::function<QString()> provider);

    void setDiagnostics(const QList<SmdDiagnostic> &diagnostics);
    void clearDiagnostics();

    void setCurrentEditor(CodeEditor *editor);
    CodeEditor *currentEditor() const { return m_currentEditor; }

signals:
    void closeRequested();
    void diagnosticsLineClicked(int line);

private:
    QPushButton *m_diagnosticsTabBtn;
    QPushButton *m_terminalTabBtn;
    QPushButton *m_judgeTabBtn;
    QPushButton *m_newTerminalBtn;
    QPushButton *m_closeBtn;
    QStackedWidget *m_stack;
    TabButtonGroup *m_tabGroup;
    TerminalPanel *m_terminalPanel;
    SubmitResultPanel *m_submitResultPanel;
    QWidget *m_headerBar = nullptr;
    QScrollArea *m_diagScrollArea = nullptr;

    // Diagnostics page
    QWidget *m_diagnosticsPage;
    DiagnosticSection *m_errorSection;
    DiagnosticSection *m_warningSection;
    QLabel *m_emptyLabel;
    QList<SmdDiagnostic> m_diagnostics;

    CodeEditor *m_currentEditor = nullptr;

    static QString tabButtonStyle(int index, bool active);
    void refreshStyle();
    void rebuildDiagnostics();
};

#endif // BOTTOMPANEL_H
