#pragma once
#include <QObject>

#include "iwp/ifactories.h"

class SimPlugin : public QObject, public iwp::ITransportFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_TRANSPORT_FACTORY_IID FILE "sim.json")
    Q_INTERFACES(iwp::ITransportFactory)
public:
    QString typeId() const override { return QStringLiteral("sim"); }
    QString displayName() const override { return tr("Simulator"); }
    QVariantMap defaultConfig() const override { return {{"mode", "scpi"}, {"latencyMs", 15}}; }
    iwp::ITransport *create() override;
};
