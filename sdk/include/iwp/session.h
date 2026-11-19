#pragma once
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>
#include <memory>
#include <vector>

#include "iwp/devicemodel.h"
#include "iwp/endpoint.h"
#include "iwp/global.h"

class QTimer;

namespace iwp {

// Cyclic tasks. A task is never started again while its previous run is
// still waiting for an answer, so a slow endpoint cannot pile up requests.
class IWP_SDK_EXPORT PollScheduler : public QObject
{
    Q_OBJECT
public:
    using Done = std::function<void()>;
    using Runner = std::function<void(Done)>;

    explicit PollScheduler(QObject *parent = nullptr);
    ~PollScheduler() override;

    void add(const QString &name, int periodMs, Runner runner);
    void start();
    void stop();

private:
    struct Task;
    void tick(Task *task);

    std::vector<std::unique_ptr<Task>> m_tasks;
    bool m_running = false;
};

// Orchestrator of one device: knows what to ask, in which order, and puts
// the answers into the DeviceModel.
//
// Lifecycle (all endpoints must be Online):
//   Idle -> Starting (onStart: identify, configure) -> Running (polling)
//   any endpoint drops -> Offline, values become stale; comes back -> Starting
//
// The session and its endpoints live in one worker thread, so endpoint
// calls are direct. Never touch widgets from here, only the model.
class IWP_SDK_EXPORT SessionBase : public QObject
{
    Q_OBJECT
public:
    using Endpoints = QHash<QString, IEndpoint *>;

    // Takes ownership of the endpoints.
    SessionBase(const Endpoints &endpoints, DeviceModel *model, QObject *parent = nullptr);
    ~SessionBase() override;

public slots:
    void start();   // endpoints must be opened after start()
    void stop();    // also closes the endpoints

protected:
    IEndpoint *endpoint(const QString &role) const { return m_endpoints.value(role, nullptr); }
    DeviceModel *model() const { return m_model; }

    // Called when all endpoints are online. Identify/configure the device,
    // then call setRunning() (or fail()). Default: start polling at once.
    virtual void onStart() { setRunning(); }
    virtual void onStopped() {}
    // A command sent by the UI through DeviceModel::sendCommand().
    virtual void onCommand(const QString &name, const QVariant &arg);

    void setRunning();
    void fail(const QString &message);

    // Request/response helpers. `role` selects the endpoint.
    void query(const QString &role, const QByteArray &command, ResponseHandler handler,
               int priority = 0, int timeoutMs = 1000);
    void write(const QString &role, const QByteArray &command, int priority = 0);
    // For devices that push data (no requests, so no request failures to
    // notice): if kick(role) is not called for timeoutMs while running, the
    // role's values become stale and the status goes Offline; the next kick
    // brings it back. Call watchdog() once in the constructor, kick() for
    // every frame received.
    void watchdog(const QString &role, int timeoutMs);
    void kick(const QString &role);
    // Cyclic query. The handler runs for every answer (also for errors).
    void poll(const QString &name, int periodMs, const QString &role,
              const QByteArray &command, ResponseHandler handler);

private:
    enum class State { Idle, Starting, Running, Offline, Failed };
    void evaluate();
    void noteResult(const QString &role, bool ok);
    void startDogs();
    void stopDogs();

    struct Dog {
        QTimer *timer = nullptr;
        int timeoutMs = 0;
        bool silent = false;
    };
    QHash<QString, Dog> m_dogs;

    Endpoints m_endpoints;
    DeviceModel *m_model;
    PollScheduler *m_scheduler;
    State m_state = State::Idle;
    bool m_started = false;
    QString m_lastError;
    QHash<QString, int> m_failures;
};

} // namespace iwp
