#pragma once
#include <QJsonObject>
#include <QStringList>
#include <functional>

#include "iwp/session.h"

// Monitors a BMC through Redfish: finds the chassis, then polls Thermal and
// Power of each. Model keys look like  "<chassis>/temp/<sensor>",
// "<chassis>/fan/<name>", "<chassis>/power_w", "<chassis>/volt/<name>",
// plus redfish_version, product, vendor.
class RedfishSession : public iwp::SessionBase
{
    Q_OBJECT
public:
    RedfishSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent = nullptr);

protected:
    void onStart() override;

private:
    void getJson(const QString &path, std::function<void(const QJsonObject &)> done);
    void addChassisPolls(const QString &chassisPath);

    bool m_pollsAdded = false;
};
