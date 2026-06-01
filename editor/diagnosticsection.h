#ifndef DIAGNOSTICSECTION_H
#define DIAGNOSTICSECTION_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QList>

#include "smd/smddiagnostic.h"

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

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

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

#endif // DIAGNOSTICSECTION_H
