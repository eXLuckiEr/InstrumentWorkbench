#include "commandedit.h"

#include <QFontDatabase>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTextBlock>

namespace { constexpr int kMaxLines = 6; }

CommandEdit::CommandEdit(QWidget *parent) : QPlainTextEdit(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabChangesFocus(true);
    connect(this, &QPlainTextEdit::textChanged, this, &CommandEdit::updateHeight);
    updateHeight();
}

void CommandEdit::updateHeight()
{
    const int lines = qBound(1, document()->blockCount(), kMaxLines);
    const int h = lines * fontMetrics().lineSpacing() + int(document()->documentMargin() * 2)
                  + 2 * frameWidth() + (horizontalScrollBar()->isVisible() ? horizontalScrollBar()->height() : 0);
    setFixedHeight(h);
    setVerticalScrollBarPolicy(document()->blockCount() > kMaxLines ? Qt::ScrollBarAsNeeded
                                                                     : Qt::ScrollBarAlwaysOff);
}

void CommandEdit::keyPressEvent(QKeyEvent *e)
{
    const bool plain = !(e->modifiers() & (Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier));
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!(e->modifiers() & Qt::ShiftModifier)) {   // Enter and Ctrl/Cmd+Enter send
            emit submitted();
            return;
        }
        break;
    case Qt::Key_Up:
        if (plain && textCursor().blockNumber() == 0 && !textCursor().hasSelection()) {
            emit historyPrevious();
            return;
        }
        break;
    case Qt::Key_Down:
        if (plain && textCursor().blockNumber() == document()->blockCount() - 1
            && !textCursor().hasSelection()) {
            emit historyNext();
            return;
        }
        break;
    default:
        break;
    }
    QPlainTextEdit::keyPressEvent(e);
}
