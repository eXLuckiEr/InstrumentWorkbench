#include "sshconfigeditor.h"

#include <QSpinBox>

SshConfigEditor::SshConfigEditor(QWidget *parent) : iwp::FormConfigEditor(parent)
{
    addText(QStringLiteral("host"), tr("Host"), QStringLiteral("192.168.0.10"));
    addInt(QStringLiteral("port"), tr("Port"), 1, 65535);
    addText(QStringLiteral("user"), tr("User"), tr("as in ssh config"));
    addFile(QStringLiteral("identityFile"), tr("Identity file"), tr("ssh-agent or ssh config"));
    addText(QStringLiteral("jumpHost"), tr("Jump host"), tr("optional, e.g. user@gateway"));
    addChoice(QStringLiteral("hostKeyChecking"), tr("Host key checking"),
              {{tr("As in ssh config"), QString()},
               {tr("Ask"), QStringLiteral("ask")},
               {tr("Accept new"), QStringLiteral("accept-new")}});
    addInt(QStringLiteral("connectTimeout"), tr("Connect timeout"), 1, 300, tr(" s"));
    addText(QStringLiteral("sshPath"), tr("ssh program"), QStringLiteral("ssh"));
}

QString SshConfigEditor::validate() const
{
    return config().value(QStringLiteral("host")).toString().isEmpty()
               ? tr("Enter the host name or address") : QString();
}
