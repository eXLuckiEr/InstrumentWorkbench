#include "nge103bplugin.h"

#include "iwp/icons.h"
#include "nge103bpage.h"
#include "nge103bsession.h"

QList<iwp::EndpointRole> Nge103bPlugin::endpointRoles() const
{
    iwp::EndpointRole scpi;
    scpi.role = QStringLiteral("scpi");
    scpi.kind = QStringLiteral("scpi");
    scpi.description = tr("SCPI command channel");
    scpi.defaults = {{"terminator", "LF"}};
    return {scpi};
}

iwp::SessionBase *Nge103bPlugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                               iwp::DeviceModel *model)
{
    return new Nge103bSession(endpoints, model);
}

QWidget *Nge103bPlugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new Nge103bPage(model, parent);
}

QIcon Nge103bPlugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/nge103b/icon.svg"));
}
