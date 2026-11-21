#pragma once
#include <QHash>
#include <QList>
#include <QVariant>

#include "iwp/canendpoint.h"
#include "iwp/session.h"

class QTimer;

// CAN bus monitor/sender. Frames from the adapter are collected and handed to the UI ~30 times
// per second as one batch (event "frames": QList<iwp::can::Frame>), together with the frames
// we sent (flag Tx). The page drives the session with commands:
//   "send"          Frame                      send one frame
//   "sequence"      QVariantList               Frame items and int pauses (ms), sent in order
//   "stopSequence"
//   "periodic"      {slot, enabled, interval, frame}   repeat a frame every `interval` ms
//   "resetStats"
// Events: "frames", "error" (text), "sequence" ({running, done, total}).
// Model keys: connection, rx, tx, fps, load (% of the bus, an estimate), dropped
class CanSession : public iwp::SessionBase
{
    Q_OBJECT
public:
    CanSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent = nullptr);

protected:
    void onStart() override;
    void onStopped() override;
    void onCommand(const QString &name, const QVariant &arg) override;

private:
    void onFrame(const iwp::can::Frame &frame);
    void publish();
    void transmit(iwp::can::Frame frame, bool quiet = false);
    void runSequence();
    void endSequence();
    void setPeriodic(const QVariantMap &arg);
    void stopPeriodic();
    void resetStats();

    iwp::CanEndpoint *m_bus = nullptr;
    QTimer *m_timer;
    QTimer *m_seqTimer;
    QList<iwp::can::Frame> m_batch;
    qsizetype m_batchLimit = 50000;

    QList<QVariant> m_seq;
    int m_seqPos = 0;
    int m_seqSent = 0;
    int m_seqFrames = 0;

    struct Periodic {
        QTimer *timer = nullptr;
        iwp::can::Frame frame;
    };
    QHash<int, Periodic> m_periodic;

    quint64 m_rx = 0;
    quint64 m_tx = 0;
    quint64 m_dropped = 0;
    quint64 m_windowFrames = 0;
    double m_windowBusSeconds = 0;
    qint64 m_windowStartMs = 0;
    double m_nominal = 500000;
    double m_dataRate = 2000000;
    int m_ticks = 0;
};
