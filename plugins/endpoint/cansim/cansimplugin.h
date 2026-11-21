#pragma once
#include <QObject>

#include "iwp/ifactories.h"

class CanSimPlugin : public QObject, public iwp::IEndpointFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_ENDPOINT_FACTORY_IID FILE "cansim.json")
    Q_INTERFACES(iwp::IEndpointFactory)
public:
    QString kind() const override { return QStringLiteral("can"); }
    QString typeId() const override { return QStringLiteral("can-sim"); }
    QString displayName() const override { return tr("CAN simulator"); }
    QVariantMap defaultConfig() const override;
    iwp::IEndpoint *create() override;
};
