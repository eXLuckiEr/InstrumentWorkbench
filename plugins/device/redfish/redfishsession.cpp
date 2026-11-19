#include "redfishsession.h"

#include <QJsonArray>
#include <QJsonDocument>

static const QString kApi = QStringLiteral("api");
static constexpr int kPollMs = 5000;

RedfishSession::RedfishSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent)
    : SessionBase(endpoints, model, parent)
{
}

void RedfishSession::getJson(const QString &path, std::function<void(const QJsonObject &)> done)
{
    query(kApi, "GET " + path.toUtf8(), [this, path, done](const iwp::Response &r) {
        if (!r.ok) {
            fail(tr("GET %1 failed: %2").arg(path, r.error));
            return;
        }
        done(QJsonDocument::fromJson(r.data).object());
    }, 0, 15000);
}

void RedfishSession::onStart()
{
    getJson(QStringLiteral("/redfish/v1/"), [this](const QJsonObject &root) {
        model()->update("redfish_version", root.value("RedfishVersion").toString(), kApi);
        model()->update("product", root.value("Product").toString(), kApi);
        model()->update("vendor", root.value("Vendor").toString(), kApi);

        getJson(QStringLiteral("/redfish/v1/Chassis"), [this](const QJsonObject &collection) {
            if (!m_pollsAdded) {   // onStart runs again after every reconnect
                m_pollsAdded = true;
                for (const QJsonValue &member : collection.value("Members").toArray()) {
                    const QString path = member.toObject().value("@odata.id").toString();
                    if (!path.isEmpty())
                        addChassisPolls(path);
                }
            }
            setRunning();
        });
    });
}

void RedfishSession::addChassisPolls(const QString &chassisPath)
{
    QString base = chassisPath;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    const QString id = base.section(QLatin1Char('/'), -1);

    poll("thermal-" + id, kPollMs, kApi, ("GET " + base + "/Thermal").toUtf8(),
         [this, id](const iwp::Response &r) {
        if (!r.ok)
            return;   // not every chassis has Thermal
        const QJsonObject o = QJsonDocument::fromJson(r.data).object();
        for (const QJsonValue &v : o.value("Temperatures").toArray()) {
            const QJsonObject t = v.toObject();
            if (t.value("ReadingCelsius").isNull() || !t.contains("ReadingCelsius"))
                continue;   // sensor without a reading
            model()->update(id + "/temp/" + t.value("Name").toString(),
                            t.value("ReadingCelsius").toDouble(), kApi);
        }
        for (const QJsonValue &v : o.value("Fans").toArray()) {
            const QJsonObject f = v.toObject();
            const QString name = f.contains("Name") ? f.value("Name").toString()
                                                    : f.value("FanName").toString();
            if (!f.contains("Reading") || f.value("Reading").isNull())
                continue;
            model()->update(id + "/fan/" + name, f.value("Reading").toDouble(), kApi);
        }
    });

    poll("power-" + id, kPollMs, kApi, ("GET " + base + "/Power").toUtf8(),
         [this, id](const iwp::Response &r) {
        if (!r.ok)
            return;
        const QJsonObject o = QJsonDocument::fromJson(r.data).object();
        const QJsonArray control = o.value("PowerControl").toArray();
        for (int i = 0; i < control.size(); ++i) {
            const QJsonValue w = control.at(i).toObject().value("PowerConsumedWatts");
            if (!w.isNull() && !w.isUndefined())
                model()->update(control.size() > 1 ? QStringLiteral("%1/power_w[%2]").arg(id).arg(i)
                                                   : id + "/power_w",
                                w.toDouble(), kApi);
        }
        for (const QJsonValue &v : o.value("Voltages").toArray()) {
            const QJsonObject volt = v.toObject();
            if (volt.contains("ReadingVolts") && !volt.value("ReadingVolts").isNull())
                model()->update(id + "/volt/" + volt.value("Name").toString(),
                                volt.value("ReadingVolts").toDouble(), kApi);
        }
    });
}
