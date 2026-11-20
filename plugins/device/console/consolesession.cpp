#include "consolesession.h"

#include <memory>

#include "iwp/framers.h"
#include "iwp/streamendpoint.h"

ConsoleSession::ConsoleSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent)
    : SessionBase(endpoints, model, parent)
{
    if (iwp::IEndpoint *ep = endpoint(QStringLiteral("main"))) {
        // A console is a raw byte stream. Whatever "terminator" a saved config carries (an old
        // default, or a hand-edited one), it must not split the stream into lines: that would swallow
        // the terminator on receive and append it to every key press on send.
        if (auto *stream = qobject_cast<iwp::StreamEndpoint *>(ep))
            stream->setFramer(std::make_unique<iwp::RawFramer>());
        connect(ep, &iwp::IEndpoint::unsolicited, this, [this](const QByteArray &data) {
            this->model()->postEvent(QStringLiteral("rx"), data);
        });
    }
}

// What the console is connected to, e.g. "192.168.0.50:5025", "user@host:22" or "COM3": the key of the
// command history of the Line view.
static QString connectionLabel(const QVariantMap &cfg)
{
    const QString host = cfg.value(QStringLiteral("host")).toString().trimmed();
    const QString port = cfg.value(QStringLiteral("port")).toString().trimmed();
    if (!host.isEmpty()) {
        const QString user = cfg.value(QStringLiteral("user")).toString().trimmed();
        return (user.isEmpty() ? QString() : user + QLatin1Char('@')) + host
               + (port.isEmpty() ? QString() : QLatin1Char(':') + port);
    }
    return port;          // serial port name; empty for anything else (the history key falls back)
}

void ConsoleSession::onStart()
{
    if (iwp::IEndpoint *ep = endpoint(QStringLiteral("main"))) {
        const QString label = connectionLabel(ep->config());
        if (!label.isEmpty())
            model()->update(QStringLiteral("connection"), label);
    }
    setRunning();
}

void ConsoleSession::onCommand(const QString &name, const QVariant &arg)
{
    if (name != QLatin1String("tx"))
        return;
    iwp::Request req;
    req.payload = arg.toByteArray();
    req.expectResponse = false;
    if (iwp::IEndpoint *ep = endpoint(QStringLiteral("main")))
        ep->submit(req, nullptr);
}
