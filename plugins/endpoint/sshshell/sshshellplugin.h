#pragma once
#include <QObject>

#include "iwp/ifactories.h"

// Interactive SSH shell as a byte stream ("stream" kind): appears next to UART and TCP in the
// console's interface list. Shares the code of plugins/endpoint/ssh (SshEndpoint in Shell mode).
class SshShellPlugin : public QObject, public iwp::IEndpointFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_ENDPOINT_FACTORY_IID FILE "sshshell.json")
    Q_INTERFACES(iwp::IEndpointFactory)
public:
    QString kind() const override { return QStringLiteral("stream"); }
    QString typeId() const override { return QStringLiteral("ssh-shell"); }
    QString displayName() const override { return tr("SSH terminal (system OpenSSH)"); }
    QVariantMap defaultConfig() const override;
    iwp::IEndpoint *create() override;
    iwp::ConfigEditor *createConfigEditor(QWidget *parent) override;
};
