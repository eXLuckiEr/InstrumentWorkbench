#pragma once
#include <QObject>

#include "iwp/ifactories.h"

class TelemetryPlugin : public QObject, public iwp::IDeviceFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_DEVICE_FACTORY_IID FILE "telemetry.json")
    Q_INTERFACES(iwp::IDeviceFactory)
public:
    QString deviceId() const override { return QStringLiteral("telemetry"); }
    QString displayName() const override { return tr("Telemetry stream"); }
    QIcon icon() const override;
    QList<iwp::EndpointRole> endpointRoles() const override;
    iwp::SessionBase *createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                    iwp::DeviceModel *model) override;
    QWidget *createPage(iwp::DeviceModel *model, QWidget *parent) override;
};
