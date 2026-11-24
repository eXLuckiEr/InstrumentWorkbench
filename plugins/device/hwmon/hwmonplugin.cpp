#include "hwmonplugin.h"

#include "hwmonpage.h"
#include "hwmonsession.h"
#include "iwp/icons.h"

QList<iwp::EndpointRole> HwmonPlugin::endpointRoles() const
{
    iwp::EndpointRole shell;
    shell.role = QStringLiteral("shell");
    shell.kind = QStringLiteral("ssh");
    shell.description = tr("SSH access to the device");
    return {shell};
}

iwp::SessionBase *HwmonPlugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                             iwp::DeviceModel *model)
{
    return new HwmonSession(endpoints, model);
}

QWidget *HwmonPlugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new HwmonPage(model, parent);
}

QIcon HwmonPlugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/hwmon/icon.svg"));
}
