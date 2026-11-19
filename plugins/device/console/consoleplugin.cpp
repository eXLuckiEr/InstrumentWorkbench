#include "consoleplugin.h"

#include "iwp/icons.h"
#include "consolepage.h"
#include "consolesession.h"

QList<iwp::EndpointRole> ConsolePlugin::endpointRoles() const
{
    iwp::EndpointRole main;
    main.role = QStringLiteral("main");
    main.kind = QStringLiteral("stream");
    main.description = tr("Raw byte stream");
    main.defaults = {{"terminator", ""}};   // raw: no framing
    return {main};
}

iwp::SessionBase *ConsolePlugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                               iwp::DeviceModel *model)
{
    return new ConsoleSession(endpoints, model);
}

QWidget *ConsolePlugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new ConsolePage(model, parent);
}

QIcon ConsolePlugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/console/icon.svg"));
}
