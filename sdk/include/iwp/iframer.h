#pragma once
#include <QByteArray>
#include <QList>
#include <QVariantMap>

#include "iwp/global.h"

namespace iwp {

// Cuts a byte stream into frames and wraps outgoing payloads.
// Used by StreamEndpoint; one instance per open endpoint, owns its buffer.
class IWP_SDK_EXPORT IFramer
{
public:
    virtual ~IFramer() = default;

    // Payload -> bytes for the wire (add terminator, header, CRC...).
    virtual QByteArray encode(const QByteArray &payload) { return payload; }

    // Append received bytes, return all frames that are complete now.
    // Must resynchronise by itself after garbage or loss of bytes.
    virtual QList<QByteArray> feed(const QByteArray &incoming) = 0;

    // Is this frame the answer to the request that is currently waiting?
    // false = push data, goes to IEndpoint::unsolicited.
    virtual bool matches(const QByteArray &frame) const
    {
        Q_UNUSED(frame)
        return true;
    }

    virtual void reset() {}

    // Diagnostics for the session, e.g. {"crcErrors": n, "droppedBytes": n}
    virtual QVariantMap stats() const { return {}; }
};

} // namespace iwp
