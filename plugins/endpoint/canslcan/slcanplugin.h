#pragma once
#include <QObject>

#include "iwp/ifactories.h"

// USB-CAN adapter in SLCAN mode (a virtual serial port): endpoint of kind "can".
class SlcanPlugin : public QObject, public iwp::IEndpointFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_ENDPOINT_FACTORY_IID FILE "canslcan.json")
    Q_INTERFACES(iwp::IEndpointFactory)
public:
    QString kind() const override { return QStringLiteral("can"); }
    QString typeId() const override { return QStringLiteral("can-slcan"); }
    QString displayName() const override { return tr("CAN adapter (SLCAN, serial port)"); }
    QVariantMap defaultConfig() const override;
    iwp::IEndpoint *create() override;
    iwp::ConfigEditor *createConfigEditor(QWidget *parent) override;
};
