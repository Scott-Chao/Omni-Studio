#ifndef HOVERMANAGER_H
#define HOVERMANAGER_H

#include <QObject>
#include <QTimer>
#include <QPoint>

#include "completionprovider.h"

class CodeEditor;
class CompletionProvider;

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

    void requestHoverAt(const QPoint &viewportPos);
    void showHoverToolTip(const HoverInfo &info);
    void hideHover();

private slots:
    void onHoverTimeout();
    void onHoverReady(HoverInfo info);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // HOVERMANAGER_H
