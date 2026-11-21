#include "mksplugin.h"

#include "iwp/icons.h"
#include "mkspage.h"
#include "mkssession.h"

QList<iwp::EndpointRole> MksPlugin::endpointRoles() const
{
    iwp::EndpointRole bus;
    bus.role = QStringLiteral("bus");
    bus.kind = QStringLiteral("can");
    bus.description = tr("CAN / CAN FD adapter (500 kbit/s by default)");
    return {bus};
}

iwp::SessionBase *MksPlugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                           iwp::DeviceModel *model)
{
    return new MksSession(endpoints, model);
}

QWidget *MksPlugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new MksPage(model, parent);
}

QIcon MksPlugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/mksservo/icon.svg"));
}
