#pragma once
#include <QElapsedTimer>

#include "iwp/itransport.h"

class QTimer;

// A software "instrument" behind a byte stream, for developing sessions and
// UIs without hardware. Config "mode":
//   "scpi" (default): line-based SCPI, LF terminated:
//       *IDN?  MEAS:VOLT?  MEAS:TEMP?  SOUR:VOLT?  SOUR:VOLT <v>  OUTP?  OUTP ON|OFF
//   "telemetry": continuous stream of 32-byte frames
//       [AA 55][counter][id][6 x float32 LE][mux u16 LE][CRC16-CCITT-FALSE LE]
//       with rare corrupted frames and lost bytes ("errorRate", default 0.002).
//       "rate": frames per second, 1..200000 (default 400; 100000 is the stress case, 3.2 MB/s).
//       "glitch": true (default) puts a one-sample spike into DATA2 every 3 s, to check that a
//       graph does not lose narrow peaks when it thins the data out.
class SimTransport : public iwp::ITransport
{
    Q_OBJECT
public:
    using iwp::ITransport::ITransport;
    QString typeId() const override { return QStringLiteral("sim"); }

protected:
    void doOpen(const QVariantMap &config) override;
    void doClose() override;
    qint64 doWrite(const QByteArray &data) override;

private:
    enum class Mode { Scpi, Telemetry };

    void handleLine(const QString &line);
    void reply(const QString &text);
    void startStream();
    void tickStream();
    void appendFrame(QByteArray &out);

    Mode m_mode = Mode::Scpi;
    QByteArray m_rx;
    int m_latencyMs = 15;
    double m_setpoint = 5.0;
    bool m_output = false;
    QElapsedTimer m_clock;

    QTimer *m_streamTimer = nullptr;
    QElapsedTimer m_streamClock;
    qint64 m_framesSent = 0;
    quint8 m_counter = 0;
    double m_errorRate = 0.002;
    int m_rate = 400;          // frames per second
    bool m_glitch = true;
};
