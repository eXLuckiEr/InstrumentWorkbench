#include "iwp/endpoint.h"

#include <QTimer>

namespace iwp {

IEndpoint::IEndpoint(QObject *parent) : QObject(parent) { m_clock.start(); }
IEndpoint::~IEndpoint() = default;

void IEndpoint::open(const QVariantMap &config)
{
    if (m_state == State::Connecting || m_state == State::Authenticating
        || m_state == State::Online)
        return;
    setState(State::Connecting);
    doOpen(config);
}

void IEndpoint::close()
{
    if (m_state == State::Offline)
        return;
    doClose();
    setState(State::Offline);
}

void IEndpoint::submit(const Request &request, ResponseHandler handler)
{
    if (m_state != State::Online) {
        Response r;
        r.error = tr("Endpoint is offline");
        QTimer::singleShot(0, this, [handler, r] {
            if (handler)
                handler(r);
        });
        return;
    }

    Pending p{m_nextId++, request, std::move(handler)};
    auto it = m_queue.begin();
    while (it != m_queue.end() && it->request.priority >= request.priority)
        ++it;
    m_queue.insert(it, std::move(p));
    pump();
}

void IEndpoint::pump()
{
    while (!m_queue.isEmpty() && m_inflight.size() < m_maxConcurrent) {
        Pending p = m_queue.takeFirst();
        const quint64 id = p.id;

        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, id] {
            doAbort(id);
            Response r;
            r.error = tr("Timeout");
            finish(id, r);
        });
        timer->start(p.request.timeoutMs);

        m_inflight.insert(id, InFlight{p.handler, m_clock.elapsed(), timer});
        doSend(id, p.request);
    }
}

void IEndpoint::finish(quint64 id, Response response)
{
    auto it = m_inflight.find(id);
    if (it == m_inflight.end())
        return;   // already timed out / failed

    const InFlight f = it.value();
    m_inflight.erase(it);
    f.timer->stop();
    f.timer->deleteLater();

    response.latencyMs = m_clock.elapsed() - f.startedMs;
    if (f.handler)
        f.handler(response);
    pump();
}

void IEndpoint::failAll(const QString &reason)
{
    Response r;
    r.error = reason;

    QList<Pending> queued;
    queued.swap(m_queue);                 // so finish() -> pump() finds nothing
    const QList<quint64> ids = m_inflight.keys();
    for (quint64 id : ids)
        finish(id, r);
    for (const Pending &p : std::as_const(queued))
        if (p.handler)
            p.handler(r);
}

void IEndpoint::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
    if (state == State::Offline || state == State::Error)
        failAll(tr("Endpoint went offline"));
}

void IEndpoint::fail(const QString &message)
{
    emit errorOccurred(message);
    setState(State::Error);
}

void IEndpoint::setMaxConcurrent(int n) { m_maxConcurrent = qMax(1, n); }

} // namespace iwp
