#include "iwp/itransport.h"

namespace iwp {

ITransport::ITransport(QObject *parent) : QObject(parent) {}
ITransport::~ITransport() = default;

void ITransport::open(const QVariantMap &config)
{
    if (m_state != State::Closed && m_state != State::Error) {
        emit errorOccurred(tr("Transport is already open"));
        return;
    }
    setState(State::Opening);
    doOpen(config);
}

void ITransport::close()
{
    if (m_state == State::Closed)
        return;
    doClose();
    setState(State::Closed);
}

void ITransport::write(const QByteArray &data)
{
    if (m_state != State::Open) {
        emit errorOccurred(tr("Transport is not open"));
        return;
    }
    if (doWrite(data) < 0)
        emit errorOccurred(tr("Write failed"));
}

void ITransport::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void ITransport::fail(const QString &message)
{
    emit errorOccurred(message);
    setState(State::Error);
}

} // namespace iwp
