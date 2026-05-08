#include "wikilinktextedit.h"

#include <QKeyEvent>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QTextBlock>

WikiLinkTextEdit::WikiLinkTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    m_model = new QStringListModel(this);
    m_completer = new QCompleter(this);
    m_completer->setModel(m_model);
    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchStartsWith);

    connect(this, &QTextEdit::cursorPositionChanged,
            this, &WikiLinkTextEdit::updateCompleter);
}

void WikiLinkTextEdit::setFileNames(const QStringList &names)
{
    m_model->setStringList(names);
}

void WikiLinkTextEdit::keyPressEvent(QKeyEvent *event)
{
    if (m_completer && m_completer->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Tab:
        {
            QAbstractItemView *pv = m_completer->popup();
            QModelIndex idx = pv->currentIndex();
            if (!idx.isValid())
                idx = pv->model()->index(0, 0);
            if (idx.isValid()) {
                QString completion = idx.data().toString();
                insertCompletion(completion);
            }
            pv->hide();
            return;
        }
        case Qt::Key_Escape:
            m_completer->popup()->hide();
            return;
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            event->ignore();
            return;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            m_completer->popup()->hide();
            break;
        default:
            break;
        }
    }

    QTextEdit::keyPressEvent(event);

    // cursorPositionChanged will fire updateCompleter automatically.
    // But arrow keys and similar non-text keys don't trigger it in all cases,
    // so also call directly for those.
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right
        || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down
        || event->key() == Qt::Key_Home || event->key() == Qt::Key_End)
    {
        updateCompleter();
    }
}

void WikiLinkTextEdit::updateCompleter()
{
    if (!m_completer)
        return;

    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    QString blockText = block.text();
    int cursorPosInBlock = cursor.positionInBlock();

    int openBracket = blockText.lastIndexOf(QStringLiteral("[["), cursorPosInBlock);
    if (openBracket < 0) {
        m_completer->popup()->hide();
        return;
    }

    int closeBracket = blockText.indexOf(QStringLiteral("]]"), openBracket + 2);
    if (closeBracket >= 0 && closeBracket < cursorPosInBlock) {
        m_completer->popup()->hide();
        return;
    }

    QString prefix = blockText.mid(openBracket + 2, cursorPosInBlock - openBracket - 2);
    m_completer->setCompletionPrefix(prefix);

    if (m_completer->completionCount() > 0) {
        QRect cr = cursorRect();
        cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                    + m_completer->popup()->verticalScrollBar()->sizeHint().width() + 20);
        m_completer->complete(cr);
        // 确保默认选中第一项
        m_completer->popup()->setCurrentIndex(
            m_completer->completionModel()->index(0, 0));
    } else {
        m_completer->popup()->hide();
    }
}

void WikiLinkTextEdit::insertCompletion(const QString &completion)
{
    QTextCursor cursor = textCursor();
    QTextBlock block = cursor.block();
    QString blockText = block.text();
    int cursorPosInBlock = cursor.positionInBlock();
    int blockStartPos = block.position();

    int openBracket = blockText.lastIndexOf(QStringLiteral("[["), cursorPosInBlock);
    if (openBracket < 0)
        return;

    int closeBracket = blockText.indexOf(QStringLiteral("]]"), openBracket + 2);
    if (closeBracket < 0)
        closeBracket = cursorPosInBlock;

    int selStart = blockStartPos + openBracket + 2;
    int selEnd = blockStartPos + closeBracket;

    cursor.setPosition(selStart);
    cursor.setPosition(selEnd, QTextCursor::KeepAnchor);
    cursor.insertText(completion + QStringLiteral("]]"));

    cursor.setPosition(selStart + completion.length() + 2);
    setTextCursor(cursor);
}
