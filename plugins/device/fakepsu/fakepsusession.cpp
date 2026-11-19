#include "fakepsusession.h"

static const QString kScpi = QStringLiteral("scpi");

FakePsuSession::FakePsuSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent)
    : SessionBase(endpoints, model, parent)
{
    // Cyclic polling, started by setRunning().
    poll("volt", 200, kScpi, "MEAS:VOLT?", [this](const iwp::Response &r) {
        if (r.ok)
            this->model()->update("volt_meas", r.toDouble(), kScpi);
    });
    poll("temp", 1000, kScpi, "MEAS:TEMP?", [this](const iwp::Response &r) {
        if (r.ok)
            this->model()->update("temp", r.toDouble(), kScpi);
    });
    poll("settings", 2000, kScpi, "SOUR:VOLT?", [this](const iwp::Response &r) {
        if (r.ok)
            this->model()->update("setpoint", r.toDouble(), kScpi);
    });
    poll("output", 2000, kScpi, "OUTP?", [this](const iwp::Response &r) {
        if (r.ok)
            this->model()->update("output", r.text() == QLatin1String("1"), kScpi);
    });
}

void FakePsuSession::onStart()
{
    query(kScpi, "*IDN?", [this](const iwp::Response &r) {
        if (!r.ok) {
            fail(tr("No answer to *IDN?: %1").arg(r.error));
            return;
        }
        model()->update("idn", r.text(), kScpi);
        readSetpoint();
        readOutput();
        setRunning();
    }, 0, 2000);
}

void FakePsuSession::onCommand(const QString &name, const QVariant &arg)
{
    // User commands go first (priority 10), ahead of polling.
    if (name == QLatin1String("setpoint")) {
        write(kScpi, "SOUR:VOLT " + QByteArray::number(arg.toDouble(), 'f', 3), 10);
        readSetpoint(10);
    } else if (name == QLatin1String("output")) {
        write(kScpi, arg.toBool() ? "OUTP ON" : "OUTP OFF", 10);
        readOutput(10);
    }
}

void FakePsuSession::readSetpoint(int priority)
{
    query(kScpi, "SOUR:VOLT?", [this](const iwp::Response &r) {
        if (r.ok)
            model()->update("setpoint", r.toDouble(), kScpi);
    }, priority);
}

void FakePsuSession::readOutput(int priority)
{
    query(kScpi, "OUTP?", [this](const iwp::Response &r) {
        if (r.ok)
            model()->update("output", r.text() == QLatin1String("1"), kScpi);
    }, priority);
}
