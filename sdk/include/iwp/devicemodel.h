#pragma once
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "iwp/global.h"

namespace iwp {

class PlotBuffer;

// The only thing a device UI talks to. Lives in the UI (main) thread.
//
//   UI -> session:  sendCommand()          (queued into the session thread)
//   session -> UI:  update(), markStale(), postEvent(), setStatus()
//                   (thread safe, always applied in the UI thread)
class IWP_SDK_EXPORT DeviceModel : public QObject
{
    Q_OBJECT
public:
    enum class Status { Stopped, Connecting, Starting, Running, Offline, Error };
    Q_ENUM(Status)

    explicit DeviceModel(QObject *parent = nullptr);

    // ---- UI side (main thread) ----
    QVariant value(const QString &key, const QVariant &fallback = {}) const;
    bool isStale(const QString &key) const;
    QStringList keys() const;
    Status status() const { return m_status; }
    QString statusMessage() const { return m_message; }
    void sendCommand(const QString &name, const QVariant &arg = {});

    // Sample history for graphs (see plotbuffer.h). Create the buffer in the session's
    // constructor (it runs in the UI thread); the page finds it by name with plot(). The
    // session then feeds it with PlotBuffer::postBatch() from its own thread.
    PlotBuffer *addPlot(const QString &name, qint64 capacity = 1 << 20);
    PlotBuffer *plot(const QString &name) const { return m_plots.value(name, nullptr); }

    // ---- session side (any thread) ----
    // `source` is normally the endpoint role the value came from, so that
    // everything from a dead endpoint can be marked stale at once.
    void update(const QString &key, const QVariant &value, const QString &source = {});
    // Many values in one hop to the UI thread: use for fast streams
    // (publish a snapshot ~30 times per second instead of every frame).
    void updateMany(const QVariantMap &values, const QString &source = {});
    void markStale(const QString &source);
    void postEvent(const QString &channel, const QVariant &payload);   // log/stream data
    void setStatus(Status status, const QString &message = {});

signals:
    void valueChanged(const QString &key, const QVariant &value);
    void staleChanged(const QString &key, bool stale);
    void eventPosted(const QString &channel, const QVariant &payload);
    void statusChanged(iwp::DeviceModel::Status status, const QString &message);
    void commandRequested(const QString &name, const QVariant &arg);

private:
    void apply(const QString &key, const QVariant &value, const QString &source);

    struct Entry {
        QVariant value;
        QString source;
        bool stale = false;
    };
    QHash<QString, Entry> m_entries;
    QHash<QString, PlotBuffer *> m_plots;
    Status m_status = Status::Stopped;
    QString m_message;
};

} // namespace iwp
