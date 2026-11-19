#include "fakepsuplugin.h"

#include "iwp/icons.h"
#include "fakepsupage.h"
#include "fakepsusession.h"

QList<iwp::EndpointRole> FakePsuPlugin::endpointRoles() const
{
    iwp::EndpointRole scpi;
    scpi.role = QStringLiteral("scpi");
    scpi.kind = QStringLiteral("scpi");
    scpi.description = tr("SCPI command channel");
    scpi.defaults = {{"terminator", "LF"}};
    return {scpi};
}

iwp::SessionBase *FakePsuPlugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                               iwp::DeviceModel *model)
{
    return new FakePsuSession(endpoints, model);
}

QWidget *FakePsuPlugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new FakePsuPage(model, parent);
}

QIcon FakePsuPlugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/fakepsu/icon.svg"));
}
