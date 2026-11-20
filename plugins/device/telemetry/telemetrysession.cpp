#include "telemetrysession.h"

#include <QtGlobal>

#include <QTimer>
#include <algorithm>
#include <utility>

#include "iwp/bytes.h"
#include "iwp/streamendpoint.h"

static const QString kData = QStringLiteral("data");
static constexpr int kFrameSize = 32;
static constexpr int kPublishMs = 33;
static constexpr int kPlotPostMs = 16;   // plot batches go out at ~60 Hz, so a 60 fps view moves smoothly

TelemetrySession::TelemetrySession(const Endpoints &endpoints, iwp::DeviceModel *model,
                                   QObject *parent)
    : SessionBase(endpoints, model, parent), m_publishTimer(new QTimer(this)),
      m_plotTimer(new QTimer(this))
{
    // Runs in the UI thread (the session is moved to its worker thread later), so the buffer
    // can be created here; the page finds it by name. Afterwards only postBatch() is used.
    m_plot = model->addPlot(QStringLiteral("signals"));
    for (int i = 1; i <= 6; ++i) {
        iwp::PlotSeriesInfo info;
        info.name = QStringLiteral("DATA%1").arg(i);
        info.regular = true;
        m_plot->addSeries(info);
    }

    // default 33 ms; IWP_TELEMETRY_PUBLISH_MS only exists for profiling experiments
    const int publishMs = qEnvironmentVariableIntValue("IWP_TELEMETRY_PUBLISH_MS");
    m_publishTimer->setInterval(publishMs > 0 ? publishMs : kPublishMs);
    connect(m_publishTimer, &QTimer::timeout, this, &TelemetrySession::publish);
    m_plotTimer->setInterval(kPlotPostMs);
    m_plotTimer->setTimerType(Qt::PreciseTimer);
    connect(m_plotTimer, &QTimer::timeout, this, &TelemetrySession::postPlot);

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
    m_kickClock.invalidate();
    m_muxDirty = 0;
    for (auto &b : m_batch)
        b.clear();
    m_plot->postClear();   // a new connection starts a new history
    m_publishTimer->start();
    m_plotTimer->start();
    setRunning();
}

void TelemetrySession::onStopped()
{
    m_publishTimer->stop();
    m_plotTimer->stop();
}

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

    for (int i = 0; i < 6; ++i) {
        const float v = iwp::readF32LE(p + 4 + 4 * i);
        m_d[i] = v;
        m_batch[i].append(v);
    }
    m_counterNow = counter;
    m_idNow = quint8(p[3]);

    // MUX DATA: rare values, a new one each frame, the cycle repeats every
    // 16 frames. ASSUMPTION: slot = counter % 16. The meaning/scale of each
    // slot (temperatures etc.) goes into a table here once it is known.
    m_mux[counter % 16] = iwp::readU16LE(p + 28);
    m_muxDirty |= quint16(1u << (counter % 16));

    m_dirty = true;
    // the watchdog timeout is 0.5 s, restarting its timer for every frame of a fast stream
    // would only waste time
    if (!m_kickClock.isValid() || m_kickClock.elapsed() >= 20) {
        m_kickClock.start();
        kick(kData);
    }
}

// Samples for the graphs: one batch per series, stamped with the time of this call.
void TelemetrySession::postPlot()
{
    if (m_batch[0].isEmpty())
        return;
    const double arrival = iwp::PlotBuffer::now();
    int biggest = 0;
    for (int i = 0; i < 6; ++i) {
        biggest = std::max(biggest, int(m_batch[i].size()));
        if (!m_batch[i].isEmpty())
            m_plot->postBatch(i, std::move(m_batch[i]), arrival);
        m_batch[i] = QVector<float>();
        m_batch[i].reserve(m_batchHint);
    }
    m_batchHint = std::max(16, biggest + biggest / 4);
}

void TelemetrySession::publish()
{
    if (!m_dirty)
        return;   // nothing new: do not mark values fresh while the stream is silent
    m_dirty = false;

    QVariantMap values;
    for (int i = 0; i < 6; ++i)
        values.insert(QStringLiteral("d%1").arg(i + 1), double(m_d[i]));
    values.insert(QStringLiteral("counter"), m_counterNow);
    values.insert(QStringLiteral("id"), m_idNow);
    for (int slot = 0; slot < 16; ++slot)
        if (m_muxDirty & (1u << slot))
            values.insert(QStringLiteral("mux%1").arg(slot), m_mux[slot]);
    m_muxDirty = 0;

    values.insert(QStringLiteral("frames_ok"), m_frames);
    values.insert(QStringLiteral("frames_lost"), m_lost);
    if (auto *stream = qobject_cast<iwp::StreamEndpoint *>(endpoint(kData))) {
        const QVariantMap st = stream->framerStats();
        values.insert(QStringLiteral("crc_errors"), st.value("crcErrors"));
        values.insert(QStringLiteral("dropped_bytes"), st.value("droppedBytes"));
    }
    const qint64 ms = m_fpsClock.elapsed();
    if (ms >= 1000) {
        values.insert(QStringLiteral("fps"), double(m_fpsFrames) * 1000.0 / double(ms));
        m_fpsFrames = 0;
        m_fpsClock.restart();
    }

    model()->updateMany(values, kData);
}
