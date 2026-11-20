#pragma once
#include "iwp/session.h"

// Degenerate session: no polling, bytes in both directions.
//   endpoint -> UI: model event "rx" (QByteArray)
//   UI -> endpoint: command "tx" (QByteArray)
class ConsoleSession : public iwp::SessionBase
{
    Q_OBJECT
public:
    ConsoleSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent = nullptr);

protected:
    void onStart() override;
    void onCommand(const QString &name, const QVariant &arg) override;
};
