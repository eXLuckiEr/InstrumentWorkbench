#include "redfishplugin.h"

#include "iwp/icons.h"
#include "iwp/keyvaluepage.h"
#include "redfishsession.h"

QList<iwp::EndpointRole> RedfishPlugin::endpointRoles() const
{
    iwp::EndpointRole api;
    api.role = QStringLiteral("api");
    api.kind = QStringLiteral("http");
    api.description = tr("Redfish service of the BMC");
    api.defaults["auth"] = "redfish-session";
    return {api};
}

iwp::SessionBase *RedfishPlugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                               iwp::DeviceModel *model)
{
    return new RedfishSession(endpoints, model);
}

QWidget *RedfishPlugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new iwp::KeyValuePage(model, parent);
}

QIcon RedfishPlugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/redfish/icon.svg"));
}
