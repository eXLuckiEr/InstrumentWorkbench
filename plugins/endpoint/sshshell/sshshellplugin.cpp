#include "sshshellplugin.h"

#include "sshconfigeditor.h"
#include "sshendpoint.h"

QVariantMap SshShellPlugin::defaultConfig() const
{
    QVariantMap cfg;
    cfg["host"] = "192.168.0.100";
    cfg["port"] = 22;
    cfg["user"] = "";               // empty = as in ~/.ssh/config or the local user name
    cfg["identityFile"] = "";
    cfg["jumpHost"] = "";
    cfg["hostKeyChecking"] = "";
    cfg["connectTimeout"] = 15;
    cfg["sshPath"] = "ssh";
    cfg["remoteCommand"] = "";      // empty = the login shell
    cfg["extraArgs"] = QStringList();
    return cfg;
}

iwp::IEndpoint *SshShellPlugin::create() { return new SshEndpoint(SshEndpoint::Mode::Shell); }

iwp::ConfigEditor *SshShellPlugin::createConfigEditor(QWidget *parent)
{
    return new SshConfigEditor(parent);
}
