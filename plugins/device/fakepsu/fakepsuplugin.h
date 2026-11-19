#pragma once
#include <QObject>

#include "iwp/ifactories.h"

class FakePsuPlugin : public QObject, public iwp::IDeviceFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_DEVICE_FACTORY_IID FILE "fakepsu.json")
    Q_INTERFACES(iwp::IDeviceFactory)
public:
    QString deviceId() const override { return QStringLiteral("fakepsu"); }
    QString displayName() const override { return tr("Fake PSU (SCPI)"); }
    QIcon icon() const override;
    QList<iwp::EndpointRole> endpointRoles() const override;
    iwp::SessionBase *createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                    iwp::DeviceModel *model) override;
    QWidget *createPage(iwp::DeviceModel *model, QWidget *parent) override;
};
