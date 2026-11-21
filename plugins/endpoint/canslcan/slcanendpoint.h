#pragma once
#include <QByteArray>
#include <QList>

#include "iwp/canendpoint.h"

class QSerialPort;
class QTimer;

// SLCAN adapter on a serial port. Opening: close the channel (if it was open), set the bit
// rate (and FD data rate, mode), open the channel; every command is acknowledged by the adapter
// with '\r' (ok) or BEL (rejected). An adapter that does not answer at all is not in SLCAN mode.
class SlcanEndpoint : public iwp::CanEndpoint
{
    Q_OBJECT
public:
    explicit SlcanEndpoint(QObject *parent = nullptr);
    ~SlcanEndpoint() override;

protected:
    void doOpen(const QVariantMap &config) override;
    void doClose() override;
    bool writeFrame(const iwp::can::Frame &frame, QString *error) override;

private:
    struct Step {
        QByteArray command;   // with '\r'
        bool required;        // failure aborts the opening
        QString what;
    };
    void onReadyRead();
    void onLine(const QByteArray &line);
    void onAck(bool ok);
    void nextStep();
    void abortOpening(const QString &message);

    QSerialPort *m_port = nullptr;
    QTimer *m_ackTimer = nullptr;
    QByteArray m_buffer;
    QList<Step> m_steps;
    bool m_opening = false;
    bool m_stepSent = false;
    bool m_heard = false;   // anything at all came from the adapter while opening
    qint64 m_lastWarnMs = -100000;
};
