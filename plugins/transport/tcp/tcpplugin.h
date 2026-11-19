#pragma once
#include <QObject>

#include "iwp/ifactories.h"

class TcpPlugin : public QObject, public iwp::ITransportFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_TRANSPORT_FACTORY_IID FILE "tcp.json")
    Q_INTERFACES(iwp::ITransportFactory)
public:
    QString typeId() const override { return QStringLiteral("tcp"); }
    QString displayName() const override { return tr("TCP client"); }
    QVariantMap defaultConfig() const override;
    iwp::ITransport *create() override;
};
