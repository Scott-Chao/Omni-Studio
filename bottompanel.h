#ifndef BOTTOMPANEL_H
#define BOTTOMPANEL_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QList>

#include "smddiagnostic.h"

class OutputPanel;
class DiagnosticSection;
class CodeEditor;

class BottomPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BottomPanel(QWidget *parent = nullptr);

    enum Tab { RunTab, DiagnosticsTab };

    OutputPanel *outputPanel() const { return m_outputPanel; }

    void showRunTab();
    void showDiagnosticsTab();
    Tab currentTab() const { return m_currentTab; }

    void setDiagnostics(const QList<SmdDiagnostic> &diagnostics);
    void clearDiagnostics();

    void setCurrentEditor(CodeEditor *editor);
    CodeEditor *currentEditor() const { return m_currentEditor; }

signals:
    void closeRequested();
    void diagnosticsLineClicked(int line);

private:
    QPushButton *m_runTabBtn;
    QPushButton *m_diagnosticsTabBtn;
    QPushButton *m_closeBtn;
    QStackedWidget *m_stack;
    OutputPanel *m_outputPanel;

    // Diagnostics page
    QWidget *m_diagnosticsPage;
    DiagnosticSection *m_errorSection;
    DiagnosticSection *m_warningSection;
    QLabel *m_emptyLabel;
    QList<SmdDiagnostic> m_diagnostics;

    Tab m_currentTab = RunTab;
    CodeEditor *m_currentEditor = nullptr;

    void updateTabButtonStyles();
    void rebuildDiagnostics();
};

#endif // BOTTOMPANEL_H
