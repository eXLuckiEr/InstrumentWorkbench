#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include "iwp/ifactories.h"

class QPluginLoader;
class QWidget;

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
        int order = 100;            // "order" in the plugin json: smaller = higher in lists
        QString group;              // "group" in the plugin json: where its devices are listed
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
    // Orders devices, transports and endpoints for the lists in the UI: by "order" from the plugin
    // json (default 100), then by display name in the current language. Call after loading and
    // after the plugins' translations are installed, before the lists are shown.
    void sort();

    const QList<iwp::ITransportFactory *> &transports() const { return m_transports; }
    const QList<iwp::IEndpointFactory *> &endpointFactories() const { return m_endpointFactories; }
    const QList<iwp::IDeviceFactory *> &devices() const { return m_devices; }
    const QList<Info> &infos() const { return m_infos; }

    // Device groups of the device list. A plugin names its group in its json ("group": "simulators");
    // known ids have a translated title and a fixed place, any other id is shown as it is (first
    // letter capitalised) after the known ones, a plugin without "group" goes to "other".
    QString groupOf(const iwp::IDeviceFactory *device) const;
    static QString groupTitle(const QString &id);
    static int groupOrder(const QString &id);

    QList<Choice> choicesFor(const QString &kind) const;
    // Creates an endpoint for the choice; nullptr if the choice is unknown.
    iwp::IEndpoint *createEndpoint(const QString &choiceId, const QString &kind) const;
    // The settings form supplied by the plugin of the choice, nullptr if it has none.
    iwp::ConfigEditor *createConfigEditor(const QString &choiceId, const QString &kind,
                                          QWidget *parent) const;

private:
    QList<iwp::ITransportFactory *> m_transports;
    QList<iwp::IEndpointFactory *> m_endpointFactories;
    QList<iwp::IDeviceFactory *> m_devices;
    QList<Info> m_infos;
    QHash<const void *, int> m_order;   // factory -> "order" of its plugin
    QHash<const void *, QString> m_group;   // factory -> "group" of its plugin
};
