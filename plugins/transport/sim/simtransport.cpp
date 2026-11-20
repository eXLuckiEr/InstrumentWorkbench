#include "simtransport.h"

#include <QRandomGenerator>
#include <QTimer>
#include <cmath>

#include "iwp/bytes.h"
#include "iwp/framers.h"

static constexpr double kPi = 3.14159265358979323846;
static constexpr int kFrameSize = 32;

void SimTransport::doOpen(const QVariantMap &config)
{
    m_mode = config.value("mode", "scpi").toString() == QLatin1String("telemetry")
                 ? Mode::Telemetry : Mode::Scpi;
    m_latencyMs = config.value("latencyMs", 15).toInt();
    m_errorRate = config.value("errorRate", 0.002).toDouble();
    m_rate = qBound(1, config.value("rate", 400).toInt(), 200000);
    m_glitch = config.value("glitch", true).toBool();
    m_rx.clear();
    m_clock.start();
    QTimer::singleShot(100, this, [this] {
        if (state() != State::Opening)
            return;
        setState(State::Open);
        if (m_mode == Mode::Telemetry)
            startStream();
    });
}

void SimTransport::doClose()
{
    if (m_streamTimer)
        m_streamTimer->stop();
}

qint64 SimTransport::doWrite(const QByteArray &data)
{
    if (m_mode != Mode::Scpi)
        return data.size();   // telemetry stream ignores anything sent to it

    m_rx += data;
    int idx;
    while ((idx = m_rx.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_rx.left(idx)).trimmed();
        m_rx.remove(0, idx + 1);
        handleLine(line);
    }
    return data.size();
}

// ---------------- SCPI mode ----------------

void SimTransport::reply(const QString &text)
{
    QTimer::singleShot(m_latencyMs, this, [this, text] {
        if (state() == State::Open)
            emit dataReceived(text.toUtf8() + "\n");
    });
}

void SimTransport::handleLine(const QString &line)
{
    const QString cmd = line.toUpper();
    const auto noise = [] { return QRandomGenerator::global()->generateDouble() - 0.5; };

    if (cmd == QLatin1String("*IDN?")) {
        reply(QStringLiteral("IWP,FakePSU,SN0001,0.1"));
    } else if (cmd == QLatin1String("MEAS:VOLT?")) {
        reply(QString::number(m_output ? m_setpoint + 0.02 * noise() : 0.0, 'f', 4));
    } else if (cmd == QLatin1String("MEAS:TEMP?")) {
        const double t = m_clock.elapsed() / 1000.0;
        const double load = m_output ? m_setpoint * 0.4 : 0.0;
        reply(QString::number(35.0 + load + 2.0 * std::sin(t / 7.0) + 0.3 * noise(), 'f', 2));
    } else if (cmd == QLatin1String("SOUR:VOLT?")) {
        reply(QString::number(m_setpoint, 'f', 3));
    } else if (cmd.startsWith(QLatin1String("SOUR:VOLT "))) {
        m_setpoint = qBound(0.0, cmd.mid(10).toDouble(), 30.0);
    } else if (cmd == QLatin1String("OUTP?")) {
        reply(m_output ? QStringLiteral("1") : QStringLiteral("0"));
    } else if (cmd == QLatin1String("OUTP ON")) {
        m_output = true;
    } else if (cmd == QLatin1String("OUTP OFF")) {
        m_output = false;
    }
    // unknown commands are silently ignored, like many real instruments do
}

// ---------------- telemetry mode ----------------

void SimTransport::startStream()
{
    if (!m_streamTimer) {
        m_streamTimer = new QTimer(this);
        m_streamTimer->setTimerType(Qt::PreciseTimer);
        m_streamTimer->setInterval(5);
        connect(m_streamTimer, &QTimer::timeout, this, &SimTransport::tickStream);
    }
    m_streamClock.start();
    m_framesSent = 0;
    m_counter = 0;
    m_streamTimer->start();
}

void SimTransport::tickStream()
{
    if (state() != State::Open)
        return;
    // Emit as many frames as are due, so the rate is right even when the
    // OS timer is coarse (Windows). All frames of one tick go out as one chunk,
    // like a UART/USB driver delivering what has accumulated.
    const qint64 due = m_streamClock.elapsed() * m_rate / 1000;
    if (due - m_framesSent > m_rate)
        m_framesSent = due - m_rate;   // never more than one second of backlog
    if (m_framesSent >= due)
        return;
    QByteArray chunk;
    chunk.reserve(int((due - m_framesSent) * kFrameSize));
    while (m_framesSent < due) {
        appendFrame(chunk);
        ++m_framesSent;
    }

    // the driver splits data at arbitrary places
    auto *rng = QRandomGenerator::global();
    if (rng->bounded(3) == 0 && chunk.size() > 8) {
        const int cut = rng->bounded(1, int(chunk.size()) - 1);
        emit dataReceived(chunk.left(cut));
        emit dataReceived(chunk.mid(cut));
    } else {
        emit dataReceived(chunk);
    }
}

void SimTransport::appendFrame(QByteArray &out)
{
    auto *rng = QRandomGenerator::global();
    QByteArray f(kFrameSize, 0);
    f[0] = char(0xAA);
    f[1] = char(0x55);
    f[2] = char(m_counter);
    f[3] = char(1);                                  // id

    const double t = double(m_framesSent) / m_rate;
    static const double freq[6] = {1.0, 2.0, 3.0, 5.0, 0.5, 0.2};
    const bool spike = m_glitch && m_framesSent % (qint64(m_rate) * 3) == 0;
    for (int i = 0; i < 6; ++i) {
        double v = (i + 1) * std::sin(2 * kPi * freq[i] * t);
        if (i == 1 && spike)
            v += 8.0;
        iwp::writeF32LE(f.data() + 4 + 4 * i, float(v));
    }

    // multiplexed slot: counter % 16 selects which rare parameter this is
    const int slot = m_counter % 16;
    const quint16 mux = quint16(2000 + slot * 100 + rng->bounded(20));
    iwp::writeU16LE(f.data() + 28, mux);
    iwp::writeU16LE(f.data() + 30, iwp::crc16CcittFalse(f.constData(), 30));
    ++m_counter;

    // line defects: corrupted frame or a lost byte
    const double dice = rng->generateDouble();
    if (dice < m_errorRate) {
        const int pos = rng->bounded(kFrameSize);
        f[pos] = char(f[pos] ^ 0x5A);
    } else if (dice < 2 * m_errorRate)
        f.remove(rng->bounded(kFrameSize), 1);

    out += f;
}
