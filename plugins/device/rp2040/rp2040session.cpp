#include "rp2040session.h"

#include "rp2040parse.h"

namespace {

const QString kScpi = QStringLiteral("scpi");

// Poll periods, ms. The chip temperature changes slowly; the ADC read on the board takes a moment.
constexpr int kTempMs = 1000;
constexpr int kUptimeMs = 2000;
constexpr int kHeapMs = 5000;
constexpr int kGpioMs = 500;   // while the GPIO view is open

std::string text(const iwp::Response &r) { return r.text().toStdString(); }

} // namespace

Rp2040Session::Rp2040Session(const Endpoints &endpoints, iwp::DeviceModel *deviceModel, QObject *parent)
    : SessionBase(endpoints, deviceModel, parent)
{
    // Created here (UI thread, before the session moves to its worker); afterwards only postSample().
    m_plot = deviceModel->addPlot(QStringLiteral("temperature"), 1 << 14);
    iwp::PlotSeriesInfo info;
    info.name = tr("Board temperature");
    info.unit = QStringLiteral("°C");
    info.color = QColor(0xe65100);
    info.regular = false;   // one point per poll, stamped on arrival
    m_plot->addSeries(info);

    // GPIO map: a long answer (about a kilobyte), so it is asked only while somebody looks at it
    m_gpioTimer = new QTimer(this);
    m_gpioTimer->setInterval(kGpioMs);
    connect(m_gpioTimer, &QTimer::timeout, this, &Rp2040Session::readGpio);

    poll(QStringLiteral("temp"), kTempMs, kScpi, "MEAS:TEMP?", [this](const iwp::Response &r) {
        bool ok = false;
        const double t = r.text().toDouble(&ok);
        if (!r.ok || !ok)
            return;
        model()->update(QStringLiteral("temp_c"), t, kScpi);
        m_plot->postSample(0, float(t));
    });
    poll(QStringLiteral("uptime"), kUptimeMs, kScpi, "SYST:UPT?", [this](const iwp::Response &r) {
        bool ok = false;
        const qlonglong s = r.text().toLongLong(&ok);
        if (r.ok && ok)
            model()->update(QStringLiteral("uptime_s"), s, kScpi);
    });
    poll(QStringLiteral("heap"), kHeapMs, kScpi, "SYST:HEAP?", [this](const iwp::Response &r) {
        long long freeNow = 0, freeMin = 0;
        if (r.ok && rp2040::parseTwoNumbers(text(r), freeNow, freeMin)) {
            model()->update(QStringLiteral("heap_free"), qlonglong(freeNow), kScpi);
            model()->update(QStringLiteral("heap_min"), qlonglong(freeMin), kScpi);
        }
    });
}

void Rp2040Session::onStart()
{
    m_plot->postClear();
    query(kScpi, "*IDN?", [this](const iwp::Response &r) {
        if (!r.ok) {
            fail(tr("No answer to *IDN?: %1").arg(r.error));
            return;
        }
        const rp2040::Idn idn = rp2040::parseIdn(text(r));
        if (!idn.ok) {
            fail(tr("Unexpected answer to *IDN?: %1").arg(r.text()));
            return;
        }
        auto s = [](const std::string &v) { return QString::fromStdString(v); };
        model()->update(QStringLiteral("idn"), r.text(), kScpi);
        model()->update(QStringLiteral("manufacturer"), s(idn.manufacturer), kScpi);
        model()->update(QStringLiteral("model"), s(idn.model), kScpi);
        model()->update(QStringLiteral("serial"), s(idn.serial), kScpi);
        model()->update(QStringLiteral("firmware"), s(idn.firmware), kScpi);

        // what never changes while the device runs: ask once
        query(kScpi, "SYST:VERS?", [this](const iwp::Response &v) {
            if (v.ok)
                model()->update(QStringLiteral("scpi_version"), v.text(), kScpi);
        });
        query(kScpi, "SYST:UID?", [this](const iwp::Response &v) {
            if (v.ok)
                model()->update(QStringLiteral("uid"), QString::fromStdString(rp2040::unquote(text(v))), kScpi);
        });
        query(kScpi, "SYST:CLOC?", [this](const iwp::Response &v) {
            bool ok = false;
            const qlonglong hz = v.text().toLongLong(&ok);
            if (v.ok && ok)
                model()->update(QStringLiteral("clock_hz"), hz, kScpi);
        });
        setRunning();
    }, 0, 2000);
}

void Rp2040Session::onStopped()
{
    m_gpioTimer->stop();
    m_gpioBusy = false;
}

void Rp2040Session::onCommand(const QString &name, const QVariant &arg)
{
    if (name == QLatin1String("gpioPolling")) {
        if (arg.toBool()) {
            if (!m_gpioTimer->isActive()) {
                m_gpioTimer->start();
                readGpio();
            }
        } else {
            m_gpioTimer->stop();
        }
    }
}

void Rp2040Session::readGpio()
{
    if (m_gpioBusy)
        return;
    m_gpioBusy = true;
    query(kScpi, "GPIO:MAP?", [this](const iwp::Response &r) {
        m_gpioBusy = false;
        if (!r.ok) {
            model()->update(QStringLiteral("gpio_error"),
                            tr("No answer to GPIO:MAP? (%1). Does the firmware include gpio_info?").arg(r.error), kScpi);
            return;
        }
        const std::vector<rp2040::GpioPin> pins = rp2040::parseGpioMap(text(r));
        if (pins.empty()) {
            model()->update(QStringLiteral("gpio_error"), tr("Unexpected answer to GPIO:MAP?"), kScpi);
            return;
        }
        QVariantList list;
        for (const rp2040::GpioPin &p : pins) {
            QVariantMap m;
            m[QStringLiteral("pin")] = p.pin;
            m[QStringLiteral("function")] = QString::fromStdString(p.function);
            m[QStringLiteral("output")] = p.output;
            m[QStringLiteral("level")] = p.level;
            m[QStringLiteral("pull")] = QString::fromStdString(p.pull);
            list.append(m);
        }
        model()->update(QStringLiteral("gpio_error"), QString(), kScpi);
        model()->update(QStringLiteral("gpio_map"), list, kScpi);
    }, 0, 2000);
}
