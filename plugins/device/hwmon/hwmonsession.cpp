#include "hwmonsession.h"

#include "hwmonsettings.h"

namespace {
const QString kStream = QStringLiteral("stream");
constexpr int kFlushMs = 33;
constexpr int kHelloTimeoutMs = 3000;    // no "H" from the device: send the script anyway
constexpr int kReadyTimeoutMs = 20000;   // no "R": the receiver did not start (no bash?)
constexpr int kSilenceMs = 5000;         // no samples for this long = stale
}

HwmonSession::HwmonSession(const Endpoints &endpoints, iwp::DeviceModel *deviceModel, QObject *parent)
    : SessionBase(endpoints, deviceModel, parent), m_flushTimer(new QTimer(this)), m_helloTimer(new QTimer(this)),
      m_readyTimer(new QTimer(this))
{
    // Created here (UI thread, before the session moves to its worker); afterwards only postBatch().
    m_plot = deviceModel->addPlot(QStringLiteral("sensors"), 1 << 15);
    for (int i = 0; i < hwmon::kMaxSensors; ++i) {
        iwp::PlotSeriesInfo info;
        info.name = QStringLiteral("S%1").arg(i + 1);
        info.regular = true;
        m_plot->addSeries(info);
    }
    m_batch.resize(hwmon::kMaxSensors);
    m_last.fill(0.0f, hwmon::kMaxSensors);
    m_texts.fill(QString(), hwmon::kMaxSensors);

    m_flushTimer->setInterval(kFlushMs);
    connect(m_flushTimer, &QTimer::timeout, this, &HwmonSession::flush);
    m_helloTimer->setSingleShot(true);
    m_helloTimer->setInterval(kHelloTimeoutMs);
    connect(m_helloTimer, &QTimer::timeout, this, &HwmonSession::sendScript);
    m_readyTimer->setSingleShot(true);
    m_readyTimer->setInterval(kReadyTimeoutMs);
    connect(m_readyTimer, &QTimer::timeout, this, [this] {
        fail(tr("The device did not start the receiver. Is bash available there?"));
    });

    if (iwp::IEndpoint *ep = endpoint(kStream)) {
        connect(ep, &iwp::IEndpoint::unsolicited, this, &HwmonSession::onData);
        watchdog(kStream, kSilenceMs);
    }
    loadConfig();
}

void HwmonSession::loadConfig()
{
    QString err;
    hwmon::Config cfg;
    if (HwmonSettings::load().toConfig(cfg, &err)) {
        m_cfg = cfg;
        m_cfgError.clear();
    } else {
        m_cfg = hwmon::Config();
        m_cfgError = err;
    }
}

void HwmonSession::resetLayout()
{
    m_pendingL.clear();
    m_slots.clear();
    m_divisor.clear();
    m_texts.fill(QString(), hwmon::kMaxSensors);
    m_last.fill(0.0f, hwmon::kMaxSensors);
    model()->update(QStringLiteral("layout"), QVariantList(), kStream);
    model()->update(QStringLiteral("values"), QVariantList(), kStream);
}

// The slots are known (from the L messages, or from the settings for a device that sends none).
void HwmonSession::applySlots()
{
    m_slots = hwmon::makeSlots(m_cfg, m_pendingL);
    if (m_slots.empty())
        m_slots = hwmon::declaredSlots(m_cfg);
    m_divisor.clear();
    QVariantList layout;
    for (const hwmon::Slot &sl : m_slots) {
        const hwmon::Sensor &x = m_cfg.sensors[size_t(sl.def)];
        m_divisor.append(x.divisor);
        QVariantMap e;
        e[QStringLiteral("name")] = QString::fromStdString(sl.name);
        e[QStringLiteral("unit")] = QString::fromStdString(x.unit);
        e[QStringLiteral("decimals")] = hwmon::decimalsFor(x.divisor);
        e[QStringLiteral("text")] = x.text;
        e[QStringLiteral("hasMin")] = x.hasMin;
        e[QStringLiteral("minV")] = x.minV;
        e[QStringLiteral("hasMax")] = x.hasMax;
        e[QStringLiteral("maxV")] = x.maxV;
        e[QStringLiteral("expect")] = QString::fromStdString(x.expect);
        layout.append(e);
    }
    model()->update(QStringLiteral("layout"), layout, kStream);
}

void HwmonSession::onStart()
{
    loadConfig();
    if (!m_cfgError.isEmpty()) {
        fail(m_cfgError);
        return;
    }
    m_scriptSent = false;
    m_ready = false;
    m_lines.clear();
    m_info.clear();
    model()->update(QStringLiteral("info"), m_info, kStream);
    resetLayout();
    m_plot->postClear();
    for (auto &b : m_batch)
        b.clear();
    // The receiver prints "H" when it is ready for the script; if that line does not come, try anyway.
    m_helloTimer->start();
    m_readyTimer->start();
}

void HwmonSession::onStopped()
{
    m_flushTimer->stop();
    m_helloTimer->stop();
    m_readyTimer->stop();
    m_scriptSent = false;
    m_ready = false;
}

void HwmonSession::sendScript()
{
    if (m_scriptSent)
        return;
    m_scriptSent = true;
    m_helloTimer->stop();
    const std::string framed = hwmon::frameScript(hwmon::buildScript(m_cfg));
    write(kStream, QByteArray::fromStdString(framed));
}

void HwmonSession::onCommand(const QString &name, const QVariant &)
{
    if (name != QLatin1String("reload"))
        return;
    loadConfig();
    if (!m_cfgError.isEmpty()) {
        model()->postEvent(QStringLiteral("log"), m_cfgError);
        return;
    }
    if (!m_scriptSent)
        return;   // not started yet: onStart / the timer will use the new settings
    // The running script stops on STOP and the receiver waits for the next one.
    write(kStream, "STOP\n");
    m_ready = false;
    m_info.clear();
    model()->update(QStringLiteral("info"), m_info, kStream);
    resetLayout();
    m_plot->postClear();
    for (auto &b : m_batch)
        b.clear();
    m_flushTimer->stop();
    m_scriptSent = false;
    m_readyTimer->start();
    QTimer::singleShot(300, this, &HwmonSession::sendScript);
}

void HwmonSession::onData(const QByteArray &data)
{
    std::vector<std::string> lines;
    m_lines.feed(data.constData(), size_t(data.size()), lines);
    for (const std::string &l : lines)
        onLine(l);
}

void HwmonSession::onLine(const std::string &line)
{
    hwmon::Message m;
    if (!hwmon::parseMessage(line, m))
        return;   // ssh messages, noise
    switch (m.type) {
    case 'H':
        sendScript();
        break;
    case 'I':
        if (m.fields.size() >= 2) {
            QVariantMap e;
            e[QStringLiteral("name")] = QString::fromStdString(m.fields[0]);
            e[QStringLiteral("text")] = QString::fromStdString(hwmon::unescapeInfo(m.fields[1])).trimmed();
            m_info.append(e);
            model()->update(QStringLiteral("info"), m_info, kStream);
        }
        break;
    case 'E':
        if (!m.fields.empty())
            model()->postEvent(QStringLiteral("log"), QString::fromStdString(m.fields[0]));
        break;
    case 'L':
        m_pendingL.push_back(m.fields);
        break;
    case 'R':
        applySlots();
        m_ready = true;
        m_readyTimer->stop();
        if (m.fields.size() >= 2) {
            model()->update(QStringLiteral("sensor_count"), QString::fromStdString(m.fields[0]).toInt(), kStream);
            model()->update(QStringLiteral("period_ms"), QString::fromStdString(m.fields[1]).toLongLong() / 1000, kStream);
        }
        m_flushTimer->start();
        setRunning();
        break;
    case 'S': {
        if (!m_ready)
            break;
        hwmon::Sample s;
        if (!hwmon::parseSample(m.fields, s))
            break;
        kick(kStream);
        const int n = qMin(int(s.value.size()), qMin(int(m_slots.size()), hwmon::kMaxSensors));
        for (int i = 0; i < n; ++i) {
            if (m_cfg.sensors[size_t(m_slots[size_t(i)].def)].text) {
                if (s.ok[size_t(i)] || !s.raw[size_t(i)].empty())
                    m_texts[i] = QString::fromStdString(s.raw[size_t(i)]);
                continue;   // no graph for a text
            }
            if (s.ok[size_t(i)])
                m_last[i] = float(s.value[size_t(i)] / m_divisor[i]);
            m_batch[i].append(m_last[i]);   // a failed read repeats the previous value, the series stay regular
        }
        m_valuesDirty = true;
        break;
    }
    default:
        break;
    }
}

void HwmonSession::flush()
{
    const double now = iwp::PlotBuffer::now();
    for (int i = 0; i < hwmon::kMaxSensors; ++i) {
        if (m_batch[i].isEmpty())
            continue;
        m_plot->postBatch(i, m_batch[i], now);
        m_batch[i].clear();
    }
    if (m_valuesDirty) {
        m_valuesDirty = false;
        QVariantList v;
        for (int i = 0; i < int(m_slots.size()); ++i) {
            if (m_cfg.sensors[size_t(m_slots[size_t(i)].def)].text)
                v.append(m_texts[i]);
            else
                v.append(double(m_last[i]));
        }
        model()->update(QStringLiteral("values"), v, kStream);
    }
}
