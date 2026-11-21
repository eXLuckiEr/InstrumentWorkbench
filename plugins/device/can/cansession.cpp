#include "cansession.h"

#include <QDateTime>
#include <QTimer>
#include <utility>

namespace {
constexpr int kPublishMs = 33;
constexpr int kStatsTicks = 15;   // statistics every ~0.5 s
}

CanSession::CanSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent)
    : SessionBase(endpoints, model, parent), m_timer(new QTimer(this)), m_seqTimer(new QTimer(this))
{
    qRegisterMetaType<iwp::can::Frame>("iwp::can::Frame");
    m_timer->setInterval(kPublishMs);
    connect(m_timer, &QTimer::timeout, this, &CanSession::publish);
    m_seqTimer->setSingleShot(true);
    connect(m_seqTimer, &QTimer::timeout, this, &CanSession::runSequence);

    m_bus = qobject_cast<iwp::CanEndpoint *>(endpoint(QStringLiteral("bus")));
    if (m_bus)
        connect(m_bus, &iwp::CanEndpoint::frameReceived, this, &CanSession::onFrame);
}

void CanSession::onStart()
{
    resetStats();
    m_windowStartMs = QDateTime::currentMSecsSinceEpoch();

    QString label;
    if (m_bus) {
        const QVariantMap cfg = m_bus->config();
        m_nominal = cfg.value("bitrate", 500000).toDouble();
        m_dataRate = cfg.value("dataBitrate", 2000000).toDouble();
        const QString port = cfg.value("port").toString();
        label = port.isEmpty() ? tr("simulator") : port;
        if (m_nominal > 0)
            label += tr(", %1 kbit/s").arg(m_nominal / 1000.0, 0, 'f', m_nominal < 10000 ? 1 : 0);
        if (cfg.value("fd", false).toBool())
            label += tr(", FD %1 Mbit/s").arg(m_dataRate / 1e6, 0, 'f', 0);
        if (cfg.value("listenOnly", false).toBool())
            label += tr(", listen only");
    }
    model()->update(QStringLiteral("connection"), label);
    m_timer->start();
    setRunning();
}

void CanSession::onStopped()
{
    m_timer->stop();
    m_seqTimer->stop();
    stopPeriodic();
    m_seq.clear();
}

void CanSession::resetStats()
{
    m_rx = m_tx = m_dropped = 0;
    m_windowFrames = 0;
    m_windowBusSeconds = 0;
    m_windowStartMs = QDateTime::currentMSecsSinceEpoch();
    QVariantMap v;
    v["rx"] = quint64(0);
    v["tx"] = quint64(0);
    v["fps"] = 0.0;
    v["load"] = 0.0;
    v["dropped"] = quint64(0);
    model()->updateMany(v, QStringLiteral("bus"));
}

void CanSession::onFrame(const iwp::can::Frame &frame)
{
    if (frame.tx())
        ++m_tx;
    else
        ++m_rx;
    ++m_windowFrames;
    m_windowBusSeconds += iwp::can::busTimeSeconds(frame, m_nominal, m_dataRate);
    if (m_batch.size() < m_batchLimit)
        m_batch.append(frame);
    else
        ++m_dropped;
}

void CanSession::publish()
{
    if (!m_batch.isEmpty()) {
        QList<iwp::can::Frame> out;
        out.swap(m_batch);
        model()->postEvent(QStringLiteral("frames"), QVariant::fromValue(out));
    }
    if (++m_ticks < kStatsTicks)
        return;
    m_ticks = 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double seconds = double(now - m_windowStartMs) / 1000.0;
    if (seconds <= 0)
        return;
    QVariantMap v;
    v["rx"] = m_rx;
    v["tx"] = m_tx;
    v["fps"] = double(m_windowFrames) / seconds;
    v["load"] = qMin(100.0, 100.0 * m_windowBusSeconds / seconds);
    v["dropped"] = m_dropped;
    model()->updateMany(v, QStringLiteral("bus"));
    m_windowStartMs = now;
    m_windowFrames = 0;
    m_windowBusSeconds = 0;
}

void CanSession::transmit(iwp::can::Frame frame, bool quiet)
{
    Q_UNUSED(quiet)
    if (!m_bus) {
        model()->postEvent(QStringLiteral("error"), tr("No CAN adapter"));
        return;
    }
    frame.flags |= iwp::can::Tx;
    frame.tsUs = m_bus->nowUs();
    m_bus->sendFrame(frame, [this, frame](const iwp::Response &r) {
        if (!r.ok) {
            model()->postEvent(QStringLiteral("error"), r.error);
            return;
        }
        onFrame(frame);   // our own frame goes to the log as well
    });
}

void CanSession::onCommand(const QString &name, const QVariant &arg)
{
    const QMetaType frameType = QMetaType::fromType<iwp::can::Frame>();
    if (name == QLatin1String("send")) {
        if (arg.metaType() == frameType)
            transmit(arg.value<iwp::can::Frame>());
    } else if (name == QLatin1String("sequence")) {
        m_seqTimer->stop();
        m_seq = arg.toList();
        m_seqPos = 0;
        m_seqSent = 0;
        m_seqFrames = 0;
        for (const QVariant &item : std::as_const(m_seq))
            if (item.metaType() == frameType)
                ++m_seqFrames;
        model()->postEvent(QStringLiteral("sequence"),
                           QVariantMap{{"running", true}, {"done", 0}, {"total", m_seqFrames}});
        runSequence();
    } else if (name == QLatin1String("stopSequence")) {
        endSequence();
    } else if (name == QLatin1String("periodic")) {
        setPeriodic(arg.toMap());
    } else if (name == QLatin1String("resetStats")) {
        resetStats();
    }
}

void CanSession::runSequence()
{
    const QMetaType frameType = QMetaType::fromType<iwp::can::Frame>();
    while (m_seqPos < m_seq.size()) {
        const QVariant item = m_seq.at(m_seqPos++);
        if (item.metaType() == frameType) {
            transmit(item.value<iwp::can::Frame>());
            ++m_seqSent;
        } else {
            const int ms = item.toInt();
            if (ms > 0) {
                m_seqTimer->start(ms);
                return;
            }
        }
    }
    endSequence();
}

void CanSession::endSequence()
{
    const bool wasRunning = !m_seq.isEmpty();
    m_seqTimer->stop();
    m_seq.clear();
    if (wasRunning)
        model()->postEvent(QStringLiteral("sequence"),
                           QVariantMap{{"running", false}, {"done", m_seqSent}, {"total", m_seqFrames}});
}

void CanSession::setPeriodic(const QVariantMap &arg)
{
    const int slot = arg.value("slot").toInt();
    Periodic &p = m_periodic[slot];
    if (!p.timer) {
        p.timer = new QTimer(this);
        connect(p.timer, &QTimer::timeout, this, [this, slot] {
            transmit(m_periodic.value(slot).frame);
        });
    }
    if (!arg.value("enabled").toBool()) {
        p.timer->stop();
        return;
    }
    const QVariant f = arg.value("frame");
    if (f.metaType() != QMetaType::fromType<iwp::can::Frame>())
        return;
    p.frame = f.value<iwp::can::Frame>();
    p.timer->setInterval(qMax(1, arg.value("interval", 100).toInt()));
    if (!p.timer->isActive()) {
        p.timer->start();
        transmit(p.frame);   // the first frame at once
    }
}

void CanSession::stopPeriodic()
{
    for (auto it = m_periodic.begin(); it != m_periodic.end(); ++it)
        if (it->timer)
            it->timer->stop();
}
