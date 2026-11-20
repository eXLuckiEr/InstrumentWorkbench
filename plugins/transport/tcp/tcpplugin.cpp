#include "tcpplugin.h"

#include "tcpconfigeditor.h"
#include "tcptransport.h"

QVariantMap TcpPlugin::defaultConfig() const
{
    return {{"host", "127.0.0.1"}, {"port", 5000}};
}

iwp::ITransport *TcpPlugin::create() { return new TcpTransport; }

iwp::ConfigEditor *TcpPlugin::createConfigEditor(QWidget *parent) { return new TcpConfigEditor(parent); }
