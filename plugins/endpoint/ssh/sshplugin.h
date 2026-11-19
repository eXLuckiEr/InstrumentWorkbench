#pragma once
#include <QObject>

#include "iwp/ifactories.h"

class SshPlugin : public QObject, public iwp::IEndpointFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_ENDPOINT_FACTORY_IID FILE "ssh.json")
    Q_INTERFACES(iwp::IEndpointFactory)
public:
    QString kind() const override { return QStringLiteral("ssh"); }
    QString typeId() const override { return QStringLiteral("ssh"); }
    QString displayName() const override { return tr("SSH (system OpenSSH)"); }
    QVariantMap defaultConfig() const override;
    iwp::IEndpoint *create() override;
};
