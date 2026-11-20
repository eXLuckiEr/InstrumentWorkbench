#include "iwp/session.h"

#include <QTimer>
#include <utility>

namespace iwp {

// ======================= PollScheduler =======================

struct PollScheduler::Task {
    QString name;
    int periodMs = 0;
    Runner run;
    QTimer *timer = nullptr;
    bool busy = false;
};

PollScheduler::PollScheduler(QObject *parent) : QObject(parent) {}
PollScheduler::~PollScheduler() = default;

void PollScheduler::add(const QString &name, int periodMs, Runner runner)
{
    auto task = std::make_unique<Task>();
    task->name = name;
    task->periodMs = periodMs;
    task->run = std::move(runner);
    task->timer = new QTimer(this);
    Task *raw = task.get();
    connect(task->timer, &QTimer::timeout, this, [this, raw] { tick(raw); });
    m_tasks.push_back(std::move(task));
    if (m_running)
        raw->timer->start(periodMs);
}

void PollScheduler::tick(Task *task)
{
    if (!m_running || task->busy)
        return;
    task->busy = true;
    task->run([task] { task->busy = false; });
}

void PollScheduler::start()
{
    m_running = true;
    for (auto &t : m_tasks) {
        t->timer->start(t->periodMs);
        Task *raw = t.get();
        QTimer::singleShot(0, this, [this, raw] { tick(raw); });
    }
}

void PollScheduler::stop()
{
    m_running = false;
    for (auto &t : m_tasks)
        t->timer->stop();
}

// ======================= SessionBase =======================

static constexpr int kStaleAfterFailures = 3;

SessionBase::SessionBase(const Endpoints &endpoints, DeviceModel *model, QObject *parent)
    : QObject(parent), m_endpoints(endpoints), m_model(model),
      m_scheduler(new PollScheduler(this))
{
    // Endpoints (and their transports) move to the worker thread together
    // with the session.
    for (IEndpoint *ep : std::as_const(m_endpoints))
        ep->setParent(this);
    connect(m_model, &DeviceModel::commandRequested, this, &SessionBase::onCommand);
}

SessionBase::~SessionBase() = default;

void SessionBase::onCommand(const QString &name, const QVariant &arg)
{
    Q_UNUSED(name)
    Q_UNUSED(arg)
}

void SessionBase::start()
{
    if (m_started)
        return;
    m_started = true;
    m_state = State::Idle;

    for (auto it = m_endpoints.cbegin(); it != m_endpoints.cend(); ++it) {
        const QString role = it.key();
        IEndpoint *ep = it.value();
        connect(ep, &IEndpoint::stateChanged, this, &SessionBase::evaluate);
        connect(ep, &IEndpoint::errorOccurred, this, [this, role](const QString &msg) {
            m_lastError = QStringLiteral("%1: %2").arg(role, msg);
            evaluate();
        });
        connect(ep, &IEndpoint::userActionRequired, this, [this](const QString &msg) {
            m_model->setStatus(DeviceModel::Status::Connecting, msg);
        });
    }
    evaluate();
}

static constexpr int kStopTimeoutMs = 1500;

void SessionBase::stop()
{
    if (m_stopping)
        return;
    if (!m_started) {
        emit stopped();
        return;
    }
    m_started = false;
    m_scheduler->stop();
    stopDogs();

    const bool online = m_state == State::Running || m_state == State::Starting;
    if (!online) {
        m_stopping = true;
        finishStop();
        return;
    }
    // Let the subclass say goodbye while the link is still up. The timer is a
    // safety net: a silent device must not block the disconnect.
    m_stopping = true;
    auto *guard = new QTimer(this);
    guard->setSingleShot(true);
    connect(guard, &QTimer::timeout, this, [this, guard] {
        guard->deleteLater();
        finishStop();
    });
    guard->start(kStopTimeoutMs);
    onStopping([this, guard] {
        guard->deleteLater();
        finishStop();
    });
}

void SessionBase::finishStop()
{
    if (!m_stopping)   // done() and the timeout both end up here; the first one wins
        return;
    m_stopping = false;
    for (IEndpoint *ep : std::as_const(m_endpoints)) {
        disconnect(ep, nullptr, this, nullptr);
        ep->close();
    }
    m_state = State::Idle;
    m_model->setStatus(DeviceModel::Status::Stopped);
    emit stopped();
}

void SessionBase::evaluate()
{
    if (!m_started)
        return;

    bool allOnline = !m_endpoints.isEmpty();
    bool anyError = false;
    for (IEndpoint *ep : std::as_const(m_endpoints)) {
        if (ep->state() != IEndpoint::State::Online)
            allOnline = false;
        if (ep->state() == IEndpoint::State::Error)
            anyError = true;
    }

    if (allOnline) {
        if (m_state == State::Idle || m_state == State::Offline) {
            m_state = State::Starting;
            m_failures.clear();
            m_model->setStatus(DeviceModel::Status::Starting);
            onStart();
        }
        return;
    }

    if (m_state == State::Starting || m_state == State::Running || m_state == State::Failed) {
        m_scheduler->stop();
        stopDogs();
        m_state = State::Offline;
        for (auto it = m_endpoints.cbegin(); it != m_endpoints.cend(); ++it)
            m_model->markStale(it.key());
        m_model->setStatus(DeviceModel::Status::Offline, m_lastError);
        onStopped();
    } else if (m_state == State::Idle) {
        m_model->setStatus(anyError ? DeviceModel::Status::Error : DeviceModel::Status::Connecting,
                           m_lastError);
    } else {   // Offline: keep the newest error text
        m_model->setStatus(DeviceModel::Status::Offline, m_lastError);
    }
}

void SessionBase::setRunning()
{
    if (m_state != State::Starting)
        return;
    m_state = State::Running;
    m_model->setStatus(DeviceModel::Status::Running);
    m_scheduler->start();
    startDogs();
}

void SessionBase::fail(const QString &message)
{
    m_state = State::Failed;
    m_scheduler->stop();
    stopDogs();
    m_model->setStatus(DeviceModel::Status::Error, message);
}

void SessionBase::watchdog(const QString &role, int timeoutMs)
{
    Dog &dog = m_dogs[role];
    dog.timeoutMs = timeoutMs;
    if (dog.timer)
        return;
    dog.timer = new QTimer(this);
    dog.timer->setSingleShot(true);
    connect(dog.timer, &QTimer::timeout, this, [this, role] {
        Dog &d = m_dogs[role];
        d.silent = true;
        m_model->markStale(role);
        m_model->setStatus(DeviceModel::Status::Offline,
                           tr("No data from '%1' for %2 ms").arg(role).arg(d.timeoutMs));
    });
}

void SessionBase::kick(const QString &role)
{
    auto it = m_dogs.find(role);
    if (it == m_dogs.end() || m_state != State::Running)
        return;
    it->timer->start(it->timeoutMs);
    if (it->silent) {
        it->silent = false;
        m_model->setStatus(DeviceModel::Status::Running);
    }
}

void SessionBase::startDogs()
{
    for (auto &dog : m_dogs) {
        dog.silent = false;
        dog.timer->start(dog.timeoutMs);
    }
}

void SessionBase::stopDogs()
{
    for (auto &dog : m_dogs)
        dog.timer->stop();
}

void SessionBase::noteResult(const QString &role, bool ok)
{
    int &n = m_failures[role];
    if (ok)
        n = 0;
    else if (++n == kStaleAfterFailures)
        m_model->markStale(role);   // values become fresh again with the next update()
}

void SessionBase::query(const QString &role, const QByteArray &command, ResponseHandler handler,
                        int priority, int timeoutMs)
{
    IEndpoint *ep = endpoint(role);
    if (!ep) {
        Response r;
        r.error = tr("No endpoint '%1'").arg(role);
        if (handler)
            handler(r);
        return;
    }
    Request req;
    req.payload = command;
    req.priority = priority;
    req.timeoutMs = timeoutMs;
    ep->submit(req, [this, role, handler](const Response &r) {
        // A protocol answer (e.g. HTTP 404) proves the link is alive.
        noteResult(role, r.ok || (r.status > 0 && r.status < 500));
        if (handler)
            handler(r);
    });
}

void SessionBase::write(const QString &role, const QByteArray &command, int priority)
{
    IEndpoint *ep = endpoint(role);
    if (!ep)
        return;
    Request req;
    req.payload = command;
    req.priority = priority;
    req.expectResponse = false;
    ep->submit(req, nullptr);
}

void SessionBase::poll(const QString &name, int periodMs, const QString &role,
                       const QByteArray &command, ResponseHandler handler)
{
    m_scheduler->add(name, periodMs, [this, role, command, handler](PollScheduler::Done done) {
        query(role, command, [handler, done](const Response &r) {
            if (handler)
                handler(r);
            done();
        });
    });
}

} // namespace iwp
