#include "nge103bsession.h"

#include <algorithm>

namespace {

const QString kScpi = QStringLiteral("scpi");

// Poll periods. Every request costs an exchange with the instrument, so rarely changing
// values are asked rarely. Lower the numbers if the display reacts too slowly.
constexpr int kFastMs = 600;    // measured voltage and current
constexpr int kMidMs = 1500;    // CV/CC mode, protection trips
constexpr int kSlowMs = 4000;   // setpoints and switches (also read back right after a command)

QByteArray number(double value) { return QByteArray::number(value, 'f', 3); }   // never locale dependent

} // namespace

Nge103bSession::Nge103bSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent)
    : SessionBase(endpoints, model, parent)
{
    // Created here (UI thread, before the session moves to its worker); afterwards only postSample().
    // Capacity 16384 points = hours of history at the poll rate.
    m_plot = model->addPlot(QStringLiteral("channels"), 1 << 14);
    static const struct { const char *suffix; const char *unit; QRgb color; } kinds[] = {
        {"V", "V", 0x1976d2}, {"I", "A", 0xe65100}, {"P", "W", 0x6a1b9a}};
    for (int ch = 1; ch <= kChannels; ++ch) {
        for (const auto &k : kinds) {
            iwp::PlotSeriesInfo info;
            info.name = QStringLiteral("CH%1 %2").arg(ch).arg(QLatin1String(k.suffix));
            info.unit = QLatin1String(k.unit);
            info.color = QColor(k.color);
            info.regular = false;
            m_plot->addSeries(info);
        }
    }

    for (int ch = 1; ch <= kChannels; ++ch) {
        const QString p = QStringLiteral("ch%1_").arg(ch);
        pollValue(kFastMs, ch, "MEAS:VOLT?", p + "v_meas", Kind::Number, Sample::Volt);
        pollValue(kFastMs, ch, "MEAS:CURR?", p + "i_meas", Kind::Number, Sample::Curr);
        pollValue(kMidMs, ch, "OUTP:MODE?", p + "mode", Kind::Text);
        pollValue(kMidMs, ch, "FUSE:TRIP?", p + "fuse_trip", Kind::Flag);
        pollValue(kMidMs, ch, "VOLT:PROT:TRIP?", p + "ovp_trip", Kind::Flag);
        pollValue(kSlowMs, ch, "VOLT?", p + "v_set", Kind::Number);
        pollValue(kSlowMs, ch, "CURR?", p + "i_set", Kind::Number);
        pollValue(kSlowMs, ch, "OUTP:SEL?", p + "sel", Kind::Flag);
        pollValue(kSlowMs, ch, "FUSE?", p + "fuse_on", Kind::Flag);
    }
    pollValue(kSlowMs, 0, "OUTP:GEN?", QStringLiteral("master"), Kind::Flag);
}

QByteArray Nge103bSession::onChannel(int channel, const QByteArray &command)
{
    return channel > 0 ? "INST OUT" + QByteArray::number(channel) + ";" + command : command;
}

QVariant Nge103bSession::parse(Kind kind, const iwp::Response &response)
{
    const QString text = response.text().trimmed();
    switch (kind) {
    case Kind::Number:
        return text.toDouble();   // "1.000E+00" is fine
    case Kind::Flag: {
        const QString t = text.toUpper();
        return t == QLatin1String("1") || t == QLatin1String("ON");   // the manual shows both forms
    }
    case Kind::Text:
        return text.toUpper();
    }
    return {};
}

void Nge103bSession::pollValue(int periodMs, int channel, const QByteArray &command, const QString &key,
                               Kind kind, Sample sample)
{
    const QByteArray full = onChannel(channel, command);
    poll(key, periodMs, kScpi, full, [this, key, kind, channel, sample](const iwp::Response &r) {
        if (!r.ok)
            return;
        const QVariant value = parse(kind, r);
        model()->update(key, value, kScpi);
        if (sample == Sample::None)
            return;
        const double x = value.toDouble();
        if (sample == Sample::Volt) {
            m_lastVolt[channel - 1] = x;
            m_plot->postSample(plotSeries(channel, kPlotVolt), float(x));
        } else {
            m_plot->postSample(plotSeries(channel, kPlotCurr), float(x));
            m_plot->postSample(plotSeries(channel, kPlotPower), float(m_lastVolt[channel - 1] * x));
        }
    });
}

void Nge103bSession::readValue(int channel, const QByteArray &command, const QString &key, Kind kind,
                               int priority)
{
    query(kScpi, onChannel(channel, command), [this, key, kind](const iwp::Response &r) {
        if (r.ok)
            model()->update(key, parse(kind, r), kScpi);
    }, priority);
}

void Nge103bSession::readChannelState(int channel, int priority)
{
    const QString p = QStringLiteral("ch%1_").arg(channel);
    readValue(channel, "VOLT?", p + "v_set", Kind::Number, priority);
    readValue(channel, "CURR?", p + "i_set", Kind::Number, priority);
    readValue(channel, "OUTP:SEL?", p + "sel", Kind::Flag, priority);
    readValue(channel, "FUSE?", p + "fuse_on", Kind::Flag, priority);
}

void Nge103bSession::onStart()
{
    m_identified = false;   // the instrument may have been replaced since the last start
    m_plot->postClear();
    query(kScpi, "*IDN?", [this](const iwp::Response &r) {
        if (!r.ok) {
            fail(tr("No answer to *IDN?: %1").arg(r.error));
            return;
        }
        m_identified = true;
        model()->update("idn", r.text(), kScpi);
        for (int ch = 1; ch <= kChannels; ++ch)
            readChannelState(ch);
        readValue(0, "OUTP:GEN?", QStringLiteral("master"), Kind::Flag);
        setRunning();
    }, 0, 2000);
}

void Nge103bSession::onStopping(std::function<void()> done)
{
    // Nothing to hand back if the instrument never answered (not connected, wrong port): do not make
    // the user wait for replies that will not come.
    if (!m_identified) {
        done();
        return;
    }
    // Give the front panel back to the user. The instrument goes to REMOTE on the first
    // command and stays there (keys locked) until it is told otherwise. Outputs are left
    // as they are on purpose: switching a load off on disconnect is a separate decision.
    write(kScpi, "SYST:LOC", 20);
    // *OPC? is answered only after SYST:LOC was processed, so the reply is better than a
    // blind delay: it proves the bytes went out over the UART and the instrument acted.
    query(kScpi, "*OPC?", [done](const iwp::Response &) { done(); }, 20, 1000);
}

void Nge103bSession::onCommand(const QString &name, const QVariant &arg)
{
    // User commands go first (priority 10), ahead of polling; then the changed value is read back.
    if (name == QLatin1String("master")) {
        write(kScpi, arg.toBool() ? "OUTP:GEN ON" : "OUTP:GEN OFF", 10);
        readValue(0, "OUTP:GEN?", QStringLiteral("master"), Kind::Flag, 10);
        return;
    }

    const QVariantMap a = arg.toMap();
    const int ch = a.value("ch").toInt();
    if (ch < 1 || ch > kChannels)
        return;
    const QString p = QStringLiteral("ch%1_").arg(ch);

    if (name == QLatin1String("setVoltage")) {
        const double v = std::clamp(a.value("value").toDouble(), 0.0, 32.2);   // range of the instrument
        write(kScpi, onChannel(ch, "VOLT " + number(v)), 10);
        readValue(ch, "VOLT?", p + "v_set", Kind::Number, 10);
    } else if (name == QLatin1String("setCurrent")) {
        const double i = std::clamp(a.value("value").toDouble(), 0.003, 3.025);
        write(kScpi, onChannel(ch, "CURR " + number(i)), 10);
        readValue(ch, "CURR?", p + "i_set", Kind::Number, 10);
    } else if (name == QLatin1String("channelOn")) {
        write(kScpi, onChannel(ch, a.value("on").toBool() ? "OUTP:SEL ON" : "OUTP:SEL OFF"), 10);
        readValue(ch, "OUTP:SEL?", p + "sel", Kind::Flag, 10);
    }
}
