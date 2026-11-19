#include "linuxhostplugin.h"

#include "iwp/icons.h"
#include "iwp/keyvaluepage.h"
#include "linuxhostsession.h"

QList<iwp::EndpointRole> LinuxHostPlugin::endpointRoles() const
{
    iwp::EndpointRole shell;
    shell.role = QStringLiteral("shell");
    shell.kind = QStringLiteral("ssh");
    shell.description = tr("SSH access to the device");
    return {shell};
}

iwp::SessionBase *LinuxHostPlugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                                 iwp::DeviceModel *model)
{
    return new LinuxHostSession(endpoints, model);
}

QWidget *LinuxHostPlugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new iwp::KeyValuePage(model, parent);
}

QIcon LinuxHostPlugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/linuxhost/icon.svg"));
}
