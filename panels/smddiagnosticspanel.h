#ifndef SMDDIAGNOSTICSPANEL_H
#define SMDDIAGNOSTICSPANEL_H

#include <QFrame>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QList>

#include "smd/smddiagnostic.h"
#include "diagnosticsection.h"

class SmdEditor;

class SmdDiagnosticsPanel : public QFrame
{
    Q_OBJECT

public:
    explicit SmdDiagnosticsPanel(SmdEditor *editor, QWidget *parent = nullptr);

    using QFrame::setVisible;

    void refresh();
    void scheduleRefresh();
    void clear();
    void refreshStyle();

private:
    SmdEditor *m_editor;
    DiagnosticSection *m_errorSection;
    DiagnosticSection *m_warningSection;
    QLabel *m_emptyLabel;
    QPushButton *m_closeBtn;
    QTimer *m_refreshTimer;

    void onLineClicked(int cellIndex, int line);
};

#endif // SMDDIAGNOSTICSPANEL_H
