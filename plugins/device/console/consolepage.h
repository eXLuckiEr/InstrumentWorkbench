#pragma once
#include <QStringList>
#include <QWidget>

#include "commandhistory.h"

namespace iwp { class DeviceModel; }
class CommandEdit;
class QComboBox;
class QEvent;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QToolButton;
class HexView;
#ifdef IWP_CONSOLE_TERMINAL
class TerminalView;
#endif

// Three views of one byte stream: lines of text (SCPI commands: blocks, history), a real terminal
// (xterm.js, optional) and a hex dump.
class ConsolePage : public QWidget
{
    Q_OBJECT
public:
    ConsolePage(iwp::DeviceModel *model, QWidget *parent = nullptr);

protected:
    // Typing anywhere in the Line view goes to the input (the log, combos... never swallow it).
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void focusInput();
    enum class Origin { LineInput, HexInput, Terminal };

    QWidget *createLineTab();
    void transmit(const QByteArray &data, Origin origin);
    void received(const QByteArray &data);
    void append(const QString &prefix, const QByteArray &data);
    void updateEnabled();

    // Line view: send a block of commands, history
    void sendBlock();
    void startSequence(const QStringList &lines);
    void sendNext(int seq);
    void stopSequence();
    void updateSendButton();
    void setConnection(const QString &connection);
    void rebuildHistory();
    void updatePinState();
    void applyHistoryText(const QString &text);
    void historyStep(int older);
    void togglePin();
    void removeCurrent();
    void clearHistory();
    void loadFile();
    void savePrefs();

    iwp::DeviceModel *m_model;
    QLabel *m_status;
    QTabWidget *m_tabs;
    QPlainTextEdit *m_log;
    CommandEdit *m_input;
    QComboBox *m_ending;
    QComboBox *m_delay;
    QComboBox *m_historyBox;
    QToolButton *m_pin;
    QToolButton *m_remove;
    QPushButton *m_sendBtn;
    HexView *m_hex;
#ifdef IWP_CONSOLE_TERMINAL
    TerminalView *m_terminal;
#endif

    CommandHistory m_history;
    QStringList m_queue;
    bool m_sending = false;
    int m_seq = 0;                 // invalidates timers of a stopped sequence
    int m_navIndex = -1;           // Up/Down walk through the history; -1 = the text being typed
    QString m_draft;
    bool m_applying = false;       // the editor text is being set from the history
};
