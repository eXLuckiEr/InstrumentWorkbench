#include "tcpconfigeditor.h"

TcpConfigEditor::TcpConfigEditor(QWidget *parent) : iwp::FormConfigEditor(parent)
{
    addText(QStringLiteral("host"), tr("Host"), QStringLiteral("192.168.0.10"));
    addInt(QStringLiteral("port"), tr("Port"), 1, 65535);
}

QString TcpConfigEditor::validate() const
{
    return config().value(QStringLiteral("host")).toString().isEmpty()
               ? tr("Enter the host name or address") : QString();
}
