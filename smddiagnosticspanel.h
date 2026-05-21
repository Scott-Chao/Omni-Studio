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

#include "smddiagnostic.h"

class SmdEditor;

class DiagnosticSection : public QWidget
{
    Q_OBJECT

public:
    DiagnosticSection(const QString &title, const QString &borderColor,
                      int severity, QWidget *parent = nullptr);

    void setDiagnostics(int cellIndex, const QList<SmdDiagnostic> &diags);
    void clear();
    void refreshStyle();
    int count() const { return m_count; }
    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }

signals:
    void lineClicked(int cellIndex, int line);

private:
    QLabel *m_headerLabel;
    QWidget *m_contentWidget;
    QVBoxLayout *m_contentLayout;
    QString m_borderColor;
    QString m_title;
    int m_severity;
    int m_count = 0;
    int m_cellIndex = -1;
    bool m_expanded = true;
};

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
