#include "slcanplugin.h"

#include "slcanconfigeditor.h"
#include "slcanendpoint.h"

QVariantMap SlcanPlugin::defaultConfig() const
{
    QVariantMap cfg;
    cfg["port"] = "";
    cfg["baud"] = 115200;          // USB adapters ignore it; real UART bridges need the right value
    cfg["bitrate"] = 500000;
    cfg["fd"] = false;
    cfg["dataBitrate"] = 2000000;  // used only with "fd"
    cfg["listenOnly"] = false;     // do not acknowledge frames, never transmit
    cfg["autoRetransmit"] = true;
    cfg["initCommands"] = "";      // extra adapter commands before the bus is opened, separated by ';'
    return cfg;
}

iwp::IEndpoint *SlcanPlugin::create() { return new SlcanEndpoint; }

iwp::ConfigEditor *SlcanPlugin::createConfigEditor(QWidget *parent)
{
    return new SlcanConfigEditor(parent);
}
