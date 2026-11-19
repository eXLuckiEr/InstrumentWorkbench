#pragma once
#include "iwp/session.h"

// Model keys: idn, volt_meas, temp, setpoint, output
// Commands:   setpoint (double), output (bool)
class FakePsuSession : public iwp::SessionBase
{
    Q_OBJECT
public:
    FakePsuSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent = nullptr);

protected:
    void onStart() override;
    void onCommand(const QString &name, const QVariant &arg) override;

private:
    void readSetpoint(int priority = 0);
    void readOutput(int priority = 0);
};
