#ifndef SIGNATUREHELPMANAGER_H
#define SIGNATUREHELPMANAGER_H

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QList>
#include <QTimer>
#include <QKeyEvent>

#include "../lsp/completionprovider.h"

class CodeEditor;
class CompletionProvider;

// ============================================================
// SignatureHelpPopup — floating widget showing signature info
// ============================================================

class SignatureHelpPopup : public QWidget
{
    Q_OBJECT

public:
    explicit SignatureHelpPopup(QWidget *parent = nullptr);

    void showSignatures(const QList<SignatureInfo> &signatures, int activeIndex);
    void navigateNext();
    void navigatePrev();
    int activeIndex() const { return m_activeIndex; }
    bool isActive() const { return isVisible() && !m_signatures.isEmpty(); }

private:
    QLabel *m_headerLabel;  // overload counter + nav hint
    QLabel *m_sigLabel;     // signature with active parameter highlighted
    QLabel *m_docLabel;     // documentation
    QList<SignatureInfo> m_signatures;
    int m_activeIndex = 0;

    void updateContent();
    void updatePosition();
    void refreshStyle();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

// ============================================================
// SignatureHelpManager — monitors cursor, triggers requests,
//                        and manages the popup lifecycle
// ============================================================

class SignatureHelpManager : public QObject
{
    Q_OBJECT

public:
    explicit SignatureHelpManager(CodeEditor *editor, CompletionProvider *provider = nullptr,
                                  QObject *parent = nullptr);

    void setProvider(CompletionProvider *provider);

    bool isActive() const { return m_popup && m_popup->isActive(); }
    void navigateNext();
    void navigatePrev();
    void hide();

private:
    CodeEditor *m_editor;
    CompletionProvider *m_provider;
    SignatureHelpPopup *m_popup;
    QTimer m_debounceTimer;

    int m_openParenPos = -1;  // position of the '(' that opened signature help

    void requestSignature();
    void setPopUpVisible(bool visible);

private slots:
    void onCursorPositionChanged();
    void onSignatureHelpReady(QList<SignatureInfo> signatures, int activeIndex);
};

#endif // SIGNATUREHELPMANAGER_H
