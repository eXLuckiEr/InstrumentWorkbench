#include "iwp/canendpoint.h"

#include <cstring>
#include <utility>

namespace iwp {

CanEndpoint::CanEndpoint(QObject *parent) : IEndpoint(parent)
{
    qRegisterMetaType<iwp::can::Frame>("iwp::can::Frame");
    m_clock.start();
}

CanEndpoint::~CanEndpoint() = default;

void CanEndpoint::restartClock() { m_clock.restart(); }

quint64 CanEndpoint::nowUs() const { return quint64(m_clock.nsecsElapsed() / 1000); }

void CanEndpoint::deliver(can::Frame frame)
{
    if (frame.tsUs == 0)
        frame.tsUs = nowUs();
    emit frameReceived(frame);
}

void CanEndpoint::sendFrame(const can::Frame &frame, ResponseHandler handler)
{
    Request r;
    r.payload = QByteArray(reinterpret_cast<const char *>(&frame), int(sizeof(can::Frame)));
    r.timeoutMs = 1000;
    r.expectResponse = false;
    submit(r, std::move(handler));
}

void CanEndpoint::doSend(quint64 id, const Request &request)
{
    Response r;
    if (request.payload.size() != int(sizeof(can::Frame))) {
        r.error = tr("Internal error: bad CAN frame");
        finish(id, r);
        return;
    }
    can::Frame frame;
    std::memcpy(&frame, request.payload.constData(), sizeof(can::Frame));
    QString error;
    if (writeFrame(frame, &error))
        r.ok = true;
    else
        r.error = error.isEmpty() ? tr("The adapter did not accept the frame") : error;
    finish(id, r);
}

} // namespace iwp
