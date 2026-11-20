#include "iwp/devicemodel.h"

#include <QMetaObject>

#include "iwp/plotbuffer.h"

namespace iwp {

DeviceModel::DeviceModel(QObject *parent) : QObject(parent) {}

QVariant DeviceModel::value(const QString &key, const QVariant &fallback) const
{
    auto it = m_entries.constFind(key);
    return it == m_entries.cend() ? fallback : it->value;
}

bool DeviceModel::isStale(const QString &key) const
{
    auto it = m_entries.constFind(key);
    return it != m_entries.cend() && it->stale;
}

QStringList DeviceModel::keys() const { return m_entries.keys(); }

PlotBuffer *DeviceModel::addPlot(const QString &name, qint64 capacity)
{
    if (PlotBuffer *existing = m_plots.value(name, nullptr))
        return existing;
    auto *buffer = new PlotBuffer(capacity, this);
    m_plots.insert(name, buffer);
    return buffer;
}

void DeviceModel::sendCommand(const QString &name, const QVariant &arg)
{
    emit commandRequested(name, arg);
}

void DeviceModel::apply(const QString &key, const QVariant &value, const QString &source)
{
    Entry &e = m_entries[key];
    const bool changed = (e.value != value) || e.stale;
    const bool wasStale = e.stale;
    e.value = value;
    e.source = source;
    e.stale = false;
    if (changed)
        emit valueChanged(key, value);
    if (wasStale)
        emit staleChanged(key, false);
}

void DeviceModel::update(const QString &key, const QVariant &value, const QString &source)
{
    QMetaObject::invokeMethod(this, [this, key, value, source] {
        apply(key, value, source);
    }, Qt::QueuedConnection);
}

void DeviceModel::updateMany(const QVariantMap &values, const QString &source)
{
    if (values.isEmpty())
        return;
    QMetaObject::invokeMethod(this, [this, values, source] {
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            apply(it.key(), it.value(), source);
    }, Qt::QueuedConnection);
}

void DeviceModel::markStale(const QString &source)
{
    QMetaObject::invokeMethod(this, [this, source] {
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->source == source && !it->stale) {
                it->stale = true;
                emit staleChanged(it.key(), true);
            }
        }
    }, Qt::QueuedConnection);
}

void DeviceModel::postEvent(const QString &channel, const QVariant &payload)
{
    QMetaObject::invokeMethod(this, [this, channel, payload] {
        emit eventPosted(channel, payload);
    }, Qt::QueuedConnection);
}

void DeviceModel::setStatus(Status status, const QString &message)
{
    QMetaObject::invokeMethod(this, [this, status, message] {
        m_status = status;
        m_message = message;
        emit statusChanged(status, message);
    }, Qt::QueuedConnection);
}

} // namespace iwp
