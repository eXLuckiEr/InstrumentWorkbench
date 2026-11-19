#include "iwp/streamendpoint.h"

#include "iwp/framers.h"

namespace iwp {

StreamEndpoint::StreamEndpoint(ITransport *transport, const QString &kind, QObject *parent)
    : IEndpoint(parent), m_transport(transport), m_kind(kind)
{
    m_transport->setParent(this);

    connect(m_transport, &ITransport::stateChanged, this, [this](ITransport::State s) {
        switch (s) {
        case ITransport::State::Closed:  setState(State::Offline); break;
        case ITransport::State::Opening: setState(State::Connecting); break;
        case ITransport::State::Open:    setState(State::Online); break;
        case ITransport::State::Error:   setState(State::Error); break;
        }
    });
    connect(m_transport, &ITransport::errorOccurred, this, &IEndpoint::errorOccurred);
    connect(m_transport, &ITransport::dataReceived, this, &StreamEndpoint::onData);
}

void StreamEndpoint::setFramer(std::unique_ptr<IFramer> framer)
{
    m_framer = std::move(framer);
    m_customFramer = (m_framer != nullptr);
}

void StreamEndpoint::doOpen(const QVariantMap &config)
{
    if (m_customFramer) {
        m_framer->reset();
    } else {
        QString error;
        m_framer = framerFromConfig(config, m_kind, &error);
        if (!m_framer) {
            fail(error);
            return;
        }
    }
    m_current = 0;
    m_transport->open(config);
}

void StreamEndpoint::doClose() { m_transport->close(); }

void StreamEndpoint::doSend(quint64 id, const Request &request)
{
    m_current = request.expectResponse ? id : 0;
    m_transport->write(m_framer->encode(request.payload));
    if (!request.expectResponse) {
        Response r;
        r.ok = true;
        finish(id, r);
    }
}

void StreamEndpoint::doAbort(quint64 id)
{
    if (m_current == id) {
        m_current = 0;
        m_framer->reset();   // a late answer must not be given to the next request
    }
}

void StreamEndpoint::onData(const QByteArray &data)
{
    if (!m_framer)
        return;
    const QList<QByteArray> frames = m_framer->feed(data);
    for (const QByteArray &frame : frames) {
        if (m_current && m_framer->matches(frame)) {
            const quint64 id = m_current;
            m_current = 0;
            Response r;
            r.ok = true;
            r.data = frame;
            finish(id, r);
        } else {
            emit unsolicited(frame);
        }
    }
}

} // namespace iwp
