#include "keyrecorder.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QKeySequence>
#include <QEvent>

KeyRecorder::KeyRecorder(const QString &actionKey,
                         const QString &initialSequence,
                         QWidget *parent)
    : QWidget(parent)
    , m_actionKey(actionKey)
    , m_sequence(initialSequence)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(28);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void KeyRecorder::setKeySequence(const QString &text)
{
    m_sequence = text;
    m_state = text.isEmpty() ? Cleared : Normal;
    update();
}

QString KeyRecorder::keySequence() const
{
    return m_sequence;
}

void KeyRecorder::clearSequence()
{
    m_sequence.clear();
    m_state = Cleared;
    update();
    emit keySequenceCaptured(m_actionKey, QKeySequence());
}

void KeyRecorder::restorePreviousSequence()
{
    m_sequence = m_previousSeq;
    m_state = m_sequence.isEmpty() ? Cleared : Normal;
    update();
}

void KeyRecorder::setConflict(bool conflicted)
{
    if (m_conflict != conflicted) {
        m_conflict = conflicted;
        update();
    }
}

void KeyRecorder::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = m_state == Recording ? QColor("#2d2d2d") : QColor("transparent");
    p.fillRect(rect(), bg);

    if (m_state == Recording) {
        QPen pen(QColor("#CC7832"), 1);
        p.setPen(pen);
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 3, 3);
    }

    QColor textColor;
    QString displayText;

    switch (m_state) {
    case Normal:
        textColor = m_conflict ? QColor("#e06c75") : QColor("#569CD6");
        displayText = m_sequence;
        break;
    case Recording:
        textColor = QColor("#999999");
        displayText = tr("按下快捷键...");
        break;
    case Cleared:
        textColor = QColor("#888888");
        displayText = tr("无绑定");
        break;
    }

    p.setPen(textColor);
    QFont f = font();
    if (m_state == Cleared)
        f.setItalic(true);
    f.setPointSize(10);
    p.setFont(f);

    QRect textRect = rect().adjusted(8, 0, -8, 0);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, displayText);
}

void KeyRecorder::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_state != Recording) {
        m_previousSeq = m_sequence;
        m_state = Recording;
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void KeyRecorder::keyPressEvent(QKeyEvent *event)
{
    if (m_state != Recording) {
        QWidget::keyPressEvent(event);
        return;
    }

    event->accept();

    // Escape -> cancel, restore previous
    if (event->key() == Qt::Key_Escape) {
        m_sequence = m_previousSeq;
        m_state = m_sequence.isEmpty() ? Cleared : Normal;
        update();
        return;
    }

    // Delete/Backspace -> clear
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        clearSequence();
        return;
    }

    // Modifier-only -> ignore (stay in Recording)
    int key = event->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift ||
        key == Qt::Key_Alt || key == Qt::Key_Meta) {
        return;
    }

    // Build QKeySequence from the event
    QKeySequence ks = QKeySequence(event->key() | event->modifiers());

    m_sequence = ks.toString(QKeySequence::NativeText);
    m_state = Normal;
    update();
    emit keySequenceCaptured(m_actionKey, ks);
}

void KeyRecorder::focusOutEvent(QFocusEvent * /*event*/)
{
    if (m_state == Recording) {
        m_sequence = m_previousSeq;
        m_state = m_sequence.isEmpty() ? Cleared : Normal;
        update();
    }
}

bool KeyRecorder::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride && m_state == Recording) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}
