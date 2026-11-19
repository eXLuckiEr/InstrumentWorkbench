#include "consolesession.h"

ConsoleSession::ConsoleSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent)
    : SessionBase(endpoints, model, parent)
{
    if (iwp::IEndpoint *ep = endpoint(QStringLiteral("main"))) {
        connect(ep, &iwp::IEndpoint::unsolicited, this, [this](const QByteArray &data) {
            this->model()->postEvent(QStringLiteral("rx"), data);
        });
    }
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
