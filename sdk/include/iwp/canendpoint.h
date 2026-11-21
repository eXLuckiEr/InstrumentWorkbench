#pragma once
#include <QElapsedTimer>
#include <QList>
#include <QMetaType>

#include "iwp/cancore.h"
#include "iwp/endpoint.h"
#include "iwp/global.h"

Q_DECLARE_METATYPE(iwp::can::Frame)

namespace iwp {

// A CAN / CAN FD adapter as an endpoint of kind "can". Frames are not bytes of a stream, so
// they do not go through StreamEndpoint: a plugin (SLCAN adapter, simulator, SocketCAN...)
// derives from this class, and a device session
//   * receives frames from the signal frameReceived(),
//   * sends them with sendFrame().
// All calls from the thread the endpoint lives in (as with every endpoint).
class IWP_SDK_EXPORT CanEndpoint : public IEndpoint
{
    Q_OBJECT
public:
    explicit CanEndpoint(QObject *parent = nullptr);
    ~CanEndpoint() override;

    QString kind() const override { return QStringLiteral("can"); }

    // The handler gets ok = true when the frame was handed to the adapter, otherwise the reason.
    void sendFrame(const can::Frame &frame, ResponseHandler handler = {});
    // Microseconds since the bus was opened (the time stamp of frames).
    quint64 nowUs() const;

signals:
    void frameReceived(const iwp::can::Frame &frame);

protected:
    // Hand the frame to the adapter. False + message if that is impossible.
    virtual bool writeFrame(const can::Frame &frame, QString *error) = 0;

    // Call when the bus is open: frame time stamps start from zero.
    void restartClock();
    // A frame from the bus: stamps it (if tsUs is 0) and emits frameReceived().
    void deliver(can::Frame frame);

    void doSend(quint64 id, const Request &request) final;

private:
    QElapsedTimer m_clock;
};

} // namespace iwp
