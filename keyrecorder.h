#ifndef KEYRECORDER_H
#define KEYRECORDER_H

#include <QWidget>
#include <QKeySequence>
#include <QString>

class KeyRecorder : public QWidget
{
    Q_OBJECT
public:
    enum State { Normal, Recording, Cleared };

    explicit KeyRecorder(const QString &actionKey,
                         const QString &initialSequence,
                         QWidget *parent = nullptr);

    void setKeySequence(const QString &text);
    QString keySequence() const;
    QString actionKey() const { return m_actionKey; }
    void clearSequence();
    void restorePreviousSequence();
    State state() const { return m_state; }
    void setConflict(bool conflicted);
    bool hasConflict() const { return m_conflict; }

signals:
    void keySequenceCaptured(const QString &actionKey, const QKeySequence &ks);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool event(QEvent *event) override;

private:
    QString m_actionKey;
    QString m_sequence;
    QString m_previousSeq;
    State m_state = Normal;
    bool m_conflict = false;
};

#endif // KEYRECORDER_H
