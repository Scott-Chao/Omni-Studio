#ifndef BOTTOMPANEL_H
#define BOTTOMPANEL_H

#include <QWidget>
#include <QList>

#include "smddiagnostic.h"

class QPushButton;
class QStackedWidget;
class OutputPanel;
class DiagnosticSection;
class CodeEditor;
class QScrollArea;
class TabButtonGroup;

class BottomPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BottomPanel(QWidget *parent = nullptr);

    enum Tab { RunTab, DiagnosticsTab };

    OutputPanel *outputPanel() const { return m_outputPanel; }

    Tab currentTab() const { return static_cast<Tab>(m_tabGroup->currentIndex()); }

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
    TabButtonGroup *m_tabGroup;
    OutputPanel *m_outputPanel;
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
