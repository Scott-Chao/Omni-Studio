#ifndef HOVERMANAGER_H
#define HOVERMANAGER_H

#include <QObject>
#include <QTimer>
#include <QPoint>

#include "../lsp/completionprovider.h"

class CodeEditor;
class CompletionProvider;
class QFrame;
class QLabel;
class QScrollArea;

class HoverManager : public QObject
{
    Q_OBJECT

public:
    explicit HoverManager(CodeEditor *editor, CompletionProvider *provider = nullptr, QObject *parent = nullptr);

    void setProvider(CompletionProvider *provider);

    bool tooltipActive() const { return m_tooltipShowing; }

private:
    CodeEditor *m_editor;
    CompletionProvider *m_provider;
    QTimer m_hoverTimer;
    QPoint m_mousePos;          // viewport-local coordinates
    int m_hoverCursorPos = -1; // text position of current/last hover request
    bool m_tooltipShowing = false;
    bool m_diagnosticTooltipActive = false;
    QString m_savedTooltipStyle;

    // Custom tooltip widget for LSP hover (supports max-height + scroll)
    QFrame *m_tooltipWidget = nullptr;
    QLabel *m_tooltipLabel = nullptr;
    QScrollArea *m_tooltipScrollArea = nullptr;
    bool m_mouseInTooltip = false;
    static constexpr int kMaxTooltipHeight = 300;

    void createTooltipWidget();
    void refreshTooltipStyle();
    bool isSameWord(int posA, int posB) const;

    void requestHoverAt(const QPoint &viewportPos);
    bool tryShowDiagnosticToolTip(const QPoint &viewportPos);
    void showHoverToolTip(const HoverInfo &info);
    void hideHover();

private slots:
    void onHoverTimeout();
    void onHoverReady(HoverInfo info);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // HOVERMANAGER_H
