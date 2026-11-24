#include "hwmonplugin.h"

#include "hwmoncore.h"
#include "hwmonpage.h"
#include "hwmonsession.h"
#include "iwp/icons.h"

QList<iwp::EndpointRole> HwmonPlugin::endpointRoles() const
{
    iwp::EndpointRole stream;
    stream.role = QStringLiteral("stream");
    stream.kind = QStringLiteral("stream");
    stream.description = tr("SSH shell of the device (choose “SSH shell”)");
    // The receiver the session talks to; the script itself is sent over the connection, nothing is
    // installed or written on the device.
    stream.defaults = {{"remoteCommand", QString::fromStdString(hwmon::bootstrapCommand())}};
    return {stream};
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
