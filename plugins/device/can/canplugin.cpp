#include "canplugin.h"

#include "canpage.h"
#include "cansession.h"
#include "iwp/icons.h"

QList<iwp::EndpointRole> CanPlugin::endpointRoles() const
{
    iwp::EndpointRole bus;
    bus.role = QStringLiteral("bus");
    bus.kind = QStringLiteral("can");
    bus.description = tr("CAN / CAN FD adapter");
    return {bus};
}

iwp::SessionBase *CanPlugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                           iwp::DeviceModel *model)
{
    return new CanSession(endpoints, model);
}

QWidget *CanPlugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new CanPage(model, parent);
}

QIcon CanPlugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/can/icon.svg"));
}
