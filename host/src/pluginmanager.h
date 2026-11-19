#pragma once
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include "iwp/ifactories.h"

class QPluginLoader;

// Finds, validates and loads plugins from a directory.
class PluginManager : public QObject
{
    Q_OBJECT
public:
    struct Info {
        QString file;
        QString id;
        QString name;
        QString version;
        bool loaded = false;
        QString error;
    };

    // A way to satisfy an endpoint role.
    struct Choice {
        QString id;             // "transport/tcp", "endpoint/http"
        QString label;
        QVariantMap defaults;
    };

    explicit PluginManager(QObject *parent = nullptr);

    void loadFrom(const QString &directory);

    const QList<iwp::ITransportFactory *> &transports() const { return m_transports; }
    const QList<iwp::IEndpointFactory *> &endpointFactories() const { return m_endpointFactories; }
    const QList<iwp::IDeviceFactory *> &devices() const { return m_devices; }
    const QList<Info> &infos() const { return m_infos; }

    QList<Choice> choicesFor(const QString &kind) const;
    // Creates an endpoint for the choice; nullptr if the choice is unknown.
    iwp::IEndpoint *createEndpoint(const QString &choiceId, const QString &kind) const;

private:
    QList<iwp::ITransportFactory *> m_transports;
    QList<iwp::IEndpointFactory *> m_endpointFactories;
    QList<iwp::IDeviceFactory *> m_devices;
    QList<Info> m_infos;
};
