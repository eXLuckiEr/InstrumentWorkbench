#include "telemetrysession.h"

#include <QTimer>

#include "iwp/bytes.h"
#include "iwp/streamendpoint.h"

static const QString kData = QStringLiteral("data");
static constexpr int kFrameSize = 32;
static constexpr int kPublishMs = 33;

TelemetrySession::TelemetrySession(const Endpoints &endpoints, iwp::DeviceModel *model,
                                   QObject *parent)
    : SessionBase(endpoints, model, parent), m_publishTimer(new QTimer(this))
{
    m_publishTimer->setInterval(kPublishMs);
    connect(m_publishTimer, &QTimer::timeout, this, &TelemetrySession::publish);

    if (iwp::IEndpoint *ep = endpoint(kData)) {
        connect(ep, &iwp::IEndpoint::unsolicited, this, &TelemetrySession::onFrame);
        watchdog(kData, 500);   // silence for 0.5 s = data is stale
    }
}

void TelemetrySession::onStart()
{
    m_haveCounter = false;
    m_frames = m_lost = m_fpsFrames = 0;
    m_fpsClock.start();
    m_pending.clear();
    m_publishTimer->start();
    setRunning();
}

void TelemetrySession::onStopped() { m_publishTimer->stop(); }

void TelemetrySession::onFrame(const QByteArray &frame)
{
    if (frame.size() != kFrameSize)
        return;
    const char *p = frame.constData();

    const quint8 counter = quint8(p[2]);
    if (m_haveCounter)   // gap in the counter = lost frames (wraps at 256)
        m_lost += quint8(counter - quint8(m_lastCounter + 1));
    m_lastCounter = counter;
    m_haveCounter = true;
    ++m_frames;
    ++m_fpsFrames;

    for (int i = 0; i < 6; ++i)
        m_pending[QStringLiteral("d%1").arg(i + 1)] = double(iwp::readF32LE(p + 4 + 4 * i));
    m_pending[QStringLiteral("counter")] = counter;
    m_pending[QStringLiteral("id")] = quint8(p[3]);

    // MUX DATA: rare values, a new one each frame, the cycle repeats every
    // 16 frames. ASSUMPTION: slot = counter % 16. The meaning/scale of each
    // slot (temperatures etc.) goes into a table here once it is known.
    m_pending[QStringLiteral("mux%1").arg(counter % 16)] = iwp::readU16LE(p + 28);

    m_dirty = true;
    kick(kData);
}

void TelemetrySession::publish()
{
    if (!m_dirty)
        return;   // nothing new: do not mark values fresh while the stream is silent
    m_dirty = false;

    m_pending[QStringLiteral("frames_ok")] = m_frames;
    m_pending[QStringLiteral("frames_lost")] = m_lost;
    if (auto *stream = qobject_cast<iwp::StreamEndpoint *>(endpoint(kData))) {
        const QVariantMap st = stream->framerStats();
        m_pending[QStringLiteral("crc_errors")] = st.value("crcErrors");
        m_pending[QStringLiteral("dropped_bytes")] = st.value("droppedBytes");
    }
    const qint64 ms = m_fpsClock.elapsed();
    if (ms >= 1000) {
        m_pending[QStringLiteral("fps")] = double(m_fpsFrames) * 1000.0 / double(ms);
        m_fpsFrames = 0;
        m_fpsClock.restart();
    }

    model()->updateMany(m_pending, kData);
    m_pending.clear();
}
