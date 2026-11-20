#pragma once
#include <QPlainTextEdit>

// Multi-line input of SCPI commands: Enter sends, Shift+Enter starts a new line, Up on the first line /
// Down on the last line walk through the history. The height follows the number of lines (1..6).
class CommandEdit : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CommandEdit(QWidget *parent = nullptr);

signals:
    void submitted();
    void historyPrevious();
    void historyNext();

protected:
    void keyPressEvent(QKeyEvent *e) override;

private:
    void updateHeight();
};
