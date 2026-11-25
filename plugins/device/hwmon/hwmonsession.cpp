#include "hwmonsession.h"

#include "hwmonsettings.h"

namespace {
const QString kShell = QStringLiteral("shell");
constexpr int kDiscoveryTimeoutMs = 15000;
constexpr int kMinPollTimeoutMs = 3000;
constexpr int kValuesIntervalMs = 100;
}

HwmonSession::HwmonSession(const Endpoints &endpoints, iwp::DeviceModel *deviceModel, QObject *parent)
    : SessionBase(endpoints, deviceModel, parent), m_pollTimer(new QTimer(this))
{
    // Created here (UI thread, before the session moves to its worker); afterwards only postSample().
    m_plot = deviceModel->addPlot(QStringLiteral("sensors"), 1 << 15);
    for (int i = 0; i < hwmon::kMaxSensors; ++i) {
        iwp::PlotSeriesInfo info;
        info.name = QStringLiteral("S%1").arg(i + 1);
        info.regular = false;   // polled: every point is stamped with the time its answer came
        m_plot->addSeries(info);
    }
    m_last.fill(0.0f, hwmon::kMaxSensors);
    m_texts.fill(QString(), hwmon::kMaxSensors);
    connect(m_pollTimer, &QTimer::timeout, this, &HwmonSession::pollOnce);
    loadConfig();
}

bool HwmonSession::loadConfig()
{
    QString err;
    hwmon::Config cfg;
    if (HwmonSettings::load().toConfig(cfg, &err)) {
        m_cfg = cfg;
        m_cfgError.clear();
        return true;
    }
    m_cfg = hwmon::Config();
    m_cfgError = err;
    return false;
}

void HwmonSession::resetLayout()
{
    ++m_generation;   // an answer of the previous run must not reach the new layout
    m_busy = false;
    m_pollTimer->stop();
    m_slots.clear();
    m_divisor.clear();
    m_pollScript.clear();
    m_noise.clear();
    m_texts.fill(QString(), hwmon::kMaxSensors);
    m_last.fill(0.0f, hwmon::kMaxSensors);
    m_plot->postClear();
    model()->update(QStringLiteral("layout"), QVariantList(), kShell);
    model()->update(QStringLiteral("values"), QVariantList(), kShell);
    model()->update(QStringLiteral("info"), QVariantList(), kShell);
    model()->update(QStringLiteral("diag"), QString(), kShell);
}

void HwmonSession::onStart()
{
    if (!loadConfig()) {
        fail(m_cfgError);
        return;
    }
    resetLayout();
    discover(true);
}

void HwmonSession::onStopped()
{
    ++m_generation;
    m_busy = false;
    m_pollTimer->stop();
}

void HwmonSession::onCommand(const QString &name, const QVariant &)
{
    if (name != QLatin1String("reload"))
        return;
    if (!loadConfig()) {
        model()->postEvent(QStringLiteral("log"), m_cfgError);
        return;
    }
    resetLayout();
    discover(false);
}

// Versions and the files behind the masks: one request. Then the polling starts.
void HwmonSession::discover(bool firstStart)
{
    const int generation = m_generation;
    const QByteArray script = QByteArray::fromStdString(hwmon::buildDiscoveryScript(m_cfg));
    query(kShell, script, [this, generation, firstStart](const iwp::Response &r) {
        if (generation != m_generation)
            return;
        if (!r.ok) {
            if (firstStart)
                fail(tr("Cannot run commands: %1").arg(r.error));
            else
                model()->postEvent(QStringLiteral("log"), tr("Cannot run commands: %1").arg(r.error));
            return;
        }
        const hwmon::Discovery d = hwmon::parseDiscovery(m_cfg, r.text().toStdString());

        QVariantList info;
        for (size_t i = 0; i < m_cfg.infos.size(); ++i) {
            QVariantMap e;
            e[QStringLiteral("name")] = QString::fromStdString(m_cfg.infos[i].name);
            e[QStringLiteral("text")] = QString::fromStdString(d.info[i]).trimmed();
            info.append(e);
        }
        model()->update(QStringLiteral("info"), info, kShell);
        for (const std::string &e : d.errors)
            model()->postEvent(QStringLiteral("log"), QString::fromStdString(e));
        noteStray(d.stray);

        m_slots = hwmon::makeSlots(m_cfg, d.slotMsgs);
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
        model()->update(QStringLiteral("layout"), layout, kShell);
        model()->update(QStringLiteral("sensor_count"), int(m_slots.size()), kShell);
        model()->update(QStringLiteral("period_ms"), m_cfg.periodMs, kShell);

        m_pollScript = QByteArray::fromStdString(hwmon::buildPollScript(m_slots));
        m_pollTimer->start(m_cfg.periodMs);
        m_valuesClock.start();
        if (firstStart)
            setRunning();
    }, 0, kDiscoveryTimeoutMs);
}

void HwmonSession::pollOnce()
{
    if (m_busy || m_pollScript.isEmpty())
        return;   // the previous answer has not come yet (a slow link): this tick is skipped
    m_busy = true;
    const int generation = m_generation;
    query(kShell, m_pollScript, [this, generation](const iwp::Response &r) {
        if (generation != m_generation)
            return;
        m_busy = false;
        if (r.ok)
            onSample(r.data);   // a failed request is counted by the base class (the status goes Offline)
    }, 0, qMax(kMinPollTimeoutMs, m_cfg.periodMs * 5));
}

void HwmonSession::onSample(const QByteArray &answer)
{
    hwmon::Sample s;
    std::vector<std::string> stray;
    if (!hwmon::parseSample(answer.toStdString(), s, &stray)) {
        noteStray(stray);
        return;
    }
    if (!stray.empty())
        noteStray(stray);
    const int n = qMin(int(s.value.size()), int(m_slots.size()));
    for (int i = 0; i < n; ++i) {
        if (m_cfg.sensors[size_t(m_slots[size_t(i)].def)].text) {
            if (!s.raw[size_t(i)].empty())
                m_texts[i] = QString::fromStdString(s.raw[size_t(i)]);
            continue;   // no graph for a text
        }
        if (s.ok[size_t(i)])
            m_last[i] = float(s.value[size_t(i)] / m_divisor[i]);
        m_plot->postSample(i, m_last[i]);   // a failed read repeats the previous value
    }
    if (m_valuesClock.elapsed() >= kValuesIntervalMs) {
        m_valuesClock.restart();
        publishValues();
    }
}

void HwmonSession::publishValues()
{
    QVariantList v;
    for (int i = 0; i < int(m_slots.size()); ++i) {
        if (m_cfg.sensors[size_t(m_slots[size_t(i)].def)].text)
            v.append(m_texts[i]);
        else
            v.append(double(m_last[i]));
    }
    model()->update(QStringLiteral("values"), v, kShell);
}

// Lines that are not part of the protocol (shell messages): the last few are shown, to explain a failure.
void HwmonSession::noteStray(const std::vector<std::string> &lines)
{
    for (const std::string &l : lines) {
        const QString t = QString::fromLocal8Bit(l.data(), qsizetype(qMin<size_t>(l.size(), 240))).trimmed();
        if (t.isEmpty())
            continue;
        m_noise.append(t);
        while (m_noise.size() > 8)
            m_noise.removeFirst();
    }
    model()->update(QStringLiteral("diag"), m_noise.join(QLatin1Char('\n')), kShell);
}
