#include "rp2040plugin.h"

#include "iwp/icons.h"
#include "rp2040page.h"
#include "rp2040session.h"

QList<iwp::EndpointRole> Rp2040Plugin::endpointRoles() const
{
    iwp::EndpointRole scpi;
    scpi.role = QStringLiteral("scpi");
    scpi.kind = QStringLiteral("scpi");
    scpi.description = tr("SCPI over USB-CDC");
    scpi.defaults = {{"terminator", "LF"}};
    return {scpi};
}

iwp::SessionBase *Rp2040Plugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                              iwp::DeviceModel *model)
{
    return new Rp2040Session(endpoints, model);
}

QWidget *Rp2040Plugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new Rp2040Page(model, parent);
}

QIcon Rp2040Plugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/rp2040/icon.svg"));
}
