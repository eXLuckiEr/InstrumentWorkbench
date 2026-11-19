#pragma once
#include <QElapsedTimer>
#include <QSerialPort>

#include "iwp/itransport.h"

// UART via QSerialPort. Config:
//   "port":     "COM3" | "/dev/ttyUSB0" | "/dev/cu.usbserial-XXXX" (port name or system location)
//   "baud":     115200 (any rate the OS driver supports: 460800, 921600, 1000000...)
//   "dataBits": 5..8,  "parity": none|even|odd|space|mark,
//   "stopBits": "1"|"1.5"|"2",  "flow": none|hardware|software
//   "dtr"/"rts": optional initial line states (rts only without hardware flow)
class SerialTransport : public iwp::ITransport
{
    Q_OBJECT
public:
    using iwp::ITransport::ITransport;
    QString typeId() const override { return QStringLiteral("serial"); }

protected:
    void doOpen(const QVariantMap &config) override;
    void doClose() override;
    qint64 doWrite(const QByteArray &data) override;

private:
    void ensurePort();    // created in the worker thread
    void onPortError(QSerialPort::SerialPortError error);
    static QString availablePortsText();

    QSerialPort *m_port = nullptr;
    QElapsedTimer m_noiseClock;
};
