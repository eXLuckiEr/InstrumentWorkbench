#pragma once
#include <QObject>

#include "iwp/ifactories.h"

class HttpPlugin : public QObject, public iwp::IEndpointFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_ENDPOINT_FACTORY_IID FILE "http.json")
    Q_INTERFACES(iwp::IEndpointFactory)
public:
    QString kind() const override { return QStringLiteral("http"); }
    QString typeId() const override { return QStringLiteral("http"); }
    QString displayName() const override { return tr("HTTP / Redfish API"); }
    QVariantMap defaultConfig() const override;
    iwp::IEndpoint *create() override;
};
