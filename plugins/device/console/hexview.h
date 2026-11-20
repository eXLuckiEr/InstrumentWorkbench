#pragma once
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

// Hex dump of everything received/sent plus a line to send raw bytes.
// Consecutive data of one direction is merged into one dump (a new header starts when the
// direction changes or the line was silent for a moment).
class HexView : public QWidget
{
    Q_OBJECT
public:
    explicit HexView(QWidget *parent = nullptr);

    void appendData(bool tx, const QByteArray &data);
    void setSendEnabled(bool on);

signals:
    void sendRequested(const QByteArray &data);

private:
    void send();
    void renderRow();

    QPlainTextEdit *m_log;
    QCheckBox *m_showTx;
    QLineEdit *m_input;
    QLabel *m_error;
    QWidget *m_sendRow;

    int m_dir = -1;            // 0 = RX, 1 = TX, -1 = nothing yet
    qint64 m_lastMs = 0;
    qint64 m_offset = 0;       // offset of the row being filled, within the current dump
    QByteArray m_row;          // bytes of that row (< 16)
    bool m_rowOpen = false;    // last block of the log is the partial row (replace, don't append)
};
