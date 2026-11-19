#pragma once
#include <memory>

#include "iwp/endpoint.h"
#include "iwp/iframer.h"
#include "iwp/itransport.h"

namespace iwp {

// Turns any byte-stream transport (TCP, UART...) into an endpoint. The
// framer decides what a frame is; the endpoint matches frames to requests.
//
// Framer choice, in order:
//   1. setFramer() from the device plugin (custom protocol),
//   2. config "framing": {...}  (see makeFramer()),
//   3. config "terminator": "LF" | "CR" | "CRLF" | "" | text
//      (default LF for kind "scpi", raw for others).
// Everything else in the config is passed to the transport.
//
// A frame that does not answer a waiting request (framer->matches() is
// false, or nobody waits) is emitted as `unsolicited`.
class IWP_SDK_EXPORT StreamEndpoint : public IEndpoint
{
    Q_OBJECT
public:
    // Takes ownership of the transport.
    StreamEndpoint(ITransport *transport, const QString &kind, QObject *parent = nullptr);

    QString kind() const override { return m_kind; }
    ITransport *transport() const { return m_transport; }

    void setFramer(std::unique_ptr<IFramer> framer);
    QVariantMap framerStats() const { return m_framer ? m_framer->stats() : QVariantMap(); }

protected:
    void doOpen(const QVariantMap &config) override;
    void doClose() override;
    void doSend(quint64 id, const Request &request) override;
    void doAbort(quint64 id) override;

private:
    void onData(const QByteArray &data);

    ITransport *m_transport;
    QString m_kind;
    std::unique_ptr<IFramer> m_framer;
    bool m_customFramer = false;
    quint64 m_current = 0;
};

} // namespace iwp
