#pragma once
#include <QHash>
#include <QIcon>
#include <memory>
#include <QList>
#include <QString>
#include <QVariantMap>
#include <QtPlugin>

#include "iwp/endpoint.h"
#include "iwp/iframer.h"
#include "iwp/itransport.h"

class QWidget;

namespace iwp {

class ConfigEditor;
class DeviceModel;
class SessionBase;

// What a device needs to talk to: e.g. {"scpi", "scpi"}, {"api", "http"},
// {"shell", "ssh", optional}. Kinds "scpi" and "stream" are served by any
// transport plugin (wrapped into a StreamEndpoint by the host); other kinds
// by IEndpointFactory plugins.
struct EndpointRole {
    QString role;
    QString kind;
    QString description;
    bool optional = false;
    QVariantMap defaults;   // config defaults, e.g. {"terminator": "LF"}
};

// Byte-stream channel plugin (TCP, UART, simulator...).
class ITransportFactory
{
public:
    virtual ~ITransportFactory() = default;

    virtual QString typeId() const = 0;        // "tcp", "serial", ...
    virtual QString displayName() const = 0;
    virtual QVariantMap defaultConfig() const { return {}; }
    virtual ITransport *create() = 0;          // no parent, ownership to caller
    // Optional form for the settings (see iwp/configeditor.h). nullptr = the host shows JSON.
    virtual ConfigEditor *createConfigEditor(QWidget *parent) { Q_UNUSED(parent) return nullptr; }
};

// Request/response channel plugin that is not a plain byte stream
// (NI-VISA, HTTP/Redfish, SSH...).
class IEndpointFactory
{
public:
    virtual ~IEndpointFactory() = default;

    virtual QString kind() const = 0;          // matches EndpointRole::kind
    virtual QString typeId() const = 0;        // "visa-gpib", "http", "ssh"
    virtual QString displayName() const = 0;
    virtual QVariantMap defaultConfig() const { return {}; }
    virtual IEndpoint *create() = 0;
    // Optional form for the settings (see iwp/configeditor.h). nullptr = the host shows JSON.
    virtual ConfigEditor *createConfigEditor(QWidget *parent) { Q_UNUSED(parent) return nullptr; }
};

// One concrete device: session (what to ask) + page (how it looks).
class IDeviceFactory
{
public:
    virtual ~IDeviceFactory() = default;

    virtual QString deviceId() const = 0;
    virtual QString displayName() const = 0;
    // Icon in the device list (see iwp::themedIcon). Empty = none.
    virtual QIcon icon() const { return {}; }
    virtual QList<EndpointRole> endpointRoles() const = 0;

    // Ownership of the endpoints passes to the session. Called in the main
    // thread, the host then moves the session to its own thread.
    virtual SessionBase *createSession(const QHash<QString, IEndpoint *> &endpoints,
                                       DeviceModel *model) = 0;
    // Optional: custom framing for a "stream"/"scpi" role (own protocol over a
    // byte transport). Return nullptr to use the standard config-driven framing.
    virtual std::unique_ptr<IFramer> createFramer(const QString &role)
    {
        Q_UNUSED(role)
        return nullptr;
    }
    // The page talks only to the model.
    virtual QWidget *createPage(DeviceModel *model, QWidget *parent) = 0;
};

} // namespace iwp

#define IWP_TRANSPORT_FACTORY_IID "org.iwp.ITransportFactory/2"
#define IWP_ENDPOINT_FACTORY_IID  "org.iwp.IEndpointFactory/2"
#define IWP_DEVICE_FACTORY_IID    "org.iwp.IDeviceFactory/3"
Q_DECLARE_INTERFACE(iwp::ITransportFactory, IWP_TRANSPORT_FACTORY_IID)
Q_DECLARE_INTERFACE(iwp::IEndpointFactory, IWP_ENDPOINT_FACTORY_IID)
Q_DECLARE_INTERFACE(iwp::IDeviceFactory, IWP_DEVICE_FACTORY_IID)
