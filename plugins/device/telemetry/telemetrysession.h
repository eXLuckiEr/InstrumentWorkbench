#pragma once
#include <QElapsedTimer>
#include <QVariantMap>

#include "iwp/session.h"

class QTimer;

// Push device: no polling. Every valid frame (the framer has already
// checked header and CRC) updates a "latest values" snapshot that is
// published to the UI ~30 times per second.
//
// Model keys: d1..d6 (float), counter, id, mux0..mux15 (raw u16),
//             frames_ok, frames_lost, crc_errors, dropped_bytes, fps
class TelemetrySession : public iwp::SessionBase
{
    Q_OBJECT
public:
    TelemetrySession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent = nullptr);

protected:
    void onStart() override;
    void onStopped() override;

private:
    void onFrame(const QByteArray &frame);
    void publish();

    QTimer *m_publishTimer;
    QVariantMap m_pending;
    bool m_dirty = false;

    bool m_haveCounter = false;
    quint8 m_lastCounter = 0;
    quint64 m_frames = 0;
    quint64 m_lost = 0;

    QElapsedTimer m_fpsClock;
    quint64 m_fpsFrames = 0;
};
