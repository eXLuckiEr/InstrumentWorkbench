#include "pluginmanager.h"

#include <QDir>
#include <QJsonObject>
#include <QLibrary>
#include <QCollator>
#include <QPluginLoader>
#include <algorithm>

#include "iwp/streamendpoint.h"
#include "iwp/version.h"

PluginManager::PluginManager(QObject *parent) : QObject(parent) {}

void PluginManager::loadFrom(const QString &directory)
{
    const QDir dir(directory);
    if (!dir.exists())
        return;

    for (const QString &file : dir.entryList(QDir::Files, QDir::Name)) {
        if (!QLibrary::isLibrary(file))
            continue;

        Info info;
        info.file = file;

        auto *loader = new QPluginLoader(dir.absoluteFilePath(file), this);

        // Metadata can be read without executing any plugin code.
        const QJsonObject meta = loader->metaData().value("MetaData").toObject();
        info.id = meta.value("id").toString(file);
        info.name = meta.value("name").toString(info.id);
        info.version = meta.value("version").toString();
        info.order = meta.value("order").toInt(100);

        if (meta.isEmpty()) {
            info.error = tr("not an IWP plugin (no metadata)");
        } else if (meta.value("apiVersion").toInt(-1) != IWP_API_VERSION) {
            info.error = tr("API version mismatch (plugin %1, host %2)")
                             .arg(meta.value("apiVersion").toInt(-1))
                             .arg(IWP_API_VERSION);
        } else if (QObject *obj = loader->instance()) {
            if (auto *t = qobject_cast<iwp::ITransportFactory *>(obj)) {
                m_transports.append(t);
                m_order.insert(t, info.order);
                info.loaded = true;
            }
            if (auto *e = qobject_cast<iwp::IEndpointFactory *>(obj)) {
                m_endpointFactories.append(e);
                m_order.insert(e, info.order);
                info.loaded = true;
            }
            if (auto *d = qobject_cast<iwp::IDeviceFactory *>(obj)) {
                m_devices.append(d);
                m_order.insert(d, info.order);
                info.loaded = true;
            }
            if (!info.loaded)
                info.error = tr("does not implement any known IWP interface");
        } else {
            info.error = loader->errorString();
        }

        if (!info.loaded)
            delete loader;   // also unloads the library
        m_infos.append(info);
    }
}

namespace {

template <typename T>
void sortFactories(QList<T *> &list, const QHash<const void *, int> &order)
{
    QCollator collator;                       // follows the default locale = UI language
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);            // "PSU 2" before "PSU 10"
    std::stable_sort(list.begin(), list.end(), [&](T *a, T *b) {
        const int oa = order.value(a, 100);
        const int ob = order.value(b, 100);
        if (oa != ob)
            return oa < ob;
        return collator.compare(a->displayName(), b->displayName()) < 0;
    });
}

} // namespace

void PluginManager::sort()
{
    sortFactories(m_devices, m_order);
    sortFactories(m_transports, m_order);
    sortFactories(m_endpointFactories, m_order);
}

QList<PluginManager::Choice> PluginManager::choicesFor(const QString &kind) const
{
    QList<Choice> out;
    if (kind == QLatin1String("scpi") || kind == QLatin1String("stream")) {
        for (auto *t : m_transports)
            out.append({QStringLiteral("transport/") + t->typeId(), t->displayName(),
                        t->defaultConfig()});
    }
    for (auto *e : m_endpointFactories) {
        if (e->kind() == kind)
            out.append({QStringLiteral("endpoint/") + e->typeId(), e->displayName(),
                        e->defaultConfig()});
    }
    return out;
}

iwp::IEndpoint *PluginManager::createEndpoint(const QString &choiceId, const QString &kind) const
{
    if (choiceId.startsWith(QLatin1String("transport/"))) {
        const QString id = choiceId.mid(10);
        for (auto *t : m_transports)
            if (t->typeId() == id)
                return new iwp::StreamEndpoint(t->create(), kind);
    } else if (choiceId.startsWith(QLatin1String("endpoint/"))) {
        const QString id = choiceId.mid(9);
        for (auto *e : m_endpointFactories)
            if (e->typeId() == id && e->kind() == kind)
                return e->create();
    }
    return nullptr;
}

iwp::ConfigEditor *PluginManager::createConfigEditor(const QString &choiceId, const QString &kind,
                                                     QWidget *parent) const
{
    if (choiceId.startsWith(QLatin1String("transport/"))) {
        const QString id = choiceId.mid(10);
        for (auto *t : m_transports)
            if (t->typeId() == id)
                return t->createConfigEditor(parent);
    } else if (choiceId.startsWith(QLatin1String("endpoint/"))) {
        const QString id = choiceId.mid(9);
        for (auto *e : m_endpointFactories)
            if (e->typeId() == id && e->kind() == kind)
                return e->createConfigEditor(parent);
    }
    return nullptr;
}
