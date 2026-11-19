#pragma once
#include <QObject>

#include "iwp/ifactories.h"

class SerialPlugin : public QObject, public iwp::ITransportFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_TRANSPORT_FACTORY_IID FILE "serial.json")
    Q_INTERFACES(iwp::ITransportFactory)
public:
    QString typeId() const override { return QStringLiteral("serial"); }
    QString displayName() const override { return tr("Serial port (UART)"); }
    QVariantMap defaultConfig() const override;
    iwp::ITransport *create() override;
};
