#pragma once
#include <QElapsedTimer>
#include <QVariantMap>

#include "iwp/plotbuffer.h"
#include "iwp/session.h"

class QTimer;

// Push device: no polling. Every valid frame (the framer has already
// checked header and CRC) updates a "latest values" snapshot that is
// published to the UI ~30 times per second. The DATA1..6 samples of all frames also go
// to the plot buffer "signals" (series 0..5), in one batch per publication.
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
    void postPlot();

    QTimer *m_publishTimer;
    QTimer *m_plotTimer;
    bool m_dirty = false;

    // Latest values, written per frame without any allocation (a frame may arrive 100000
    // times a second); turned into a QVariantMap only when publishing.
    float m_d[6] = {};
    quint8 m_counterNow = 0;
    quint8 m_idNow = 0;
    quint16 m_mux[16] = {};
    quint16 m_muxDirty = 0;
    iwp::PlotBuffer *m_plot = nullptr;
    QVector<float> m_batch[6];
    int m_batchHint = 64;
    QElapsedTimer m_kickClock;

    bool m_haveCounter = false;
    quint8 m_lastCounter = 0;
    quint64 m_frames = 0;
    quint64 m_lost = 0;

    QElapsedTimer m_fpsClock;
    quint64 m_fpsFrames = 0;
};
