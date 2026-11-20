#include "sshplugin.h"

#include "sshconfigeditor.h"
#include "sshendpoint.h"

QVariantMap SshPlugin::defaultConfig() const
{
    QVariantMap cfg;
    cfg["host"] = "192.168.0.100";
    cfg["port"] = 22;
    cfg["user"] = "";               // empty = as in ~/.ssh/config or the local user name
    cfg["identityFile"] = "";       // optional key (also FIDO2 "-sk" keys), else ssh-agent/config
    cfg["jumpHost"] = "";           // optional, ProxyJump
    cfg["hostKeyChecking"] = "";    // "" = as in your ssh config | ask | accept-new
    cfg["connectTimeout"] = 15;
    cfg["sshPath"] = "ssh";
    cfg["extraArgs"] = QStringList();
    return cfg;
}

iwp::IEndpoint *SshPlugin::create() { return new SshEndpoint; }

iwp::ConfigEditor *SshPlugin::createConfigEditor(QWidget *parent) { return new SshConfigEditor(parent); }
