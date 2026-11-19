#pragma once
#include <QHash>
#include <QObject>
#include <QThread>
#include <QVariantMap>

#include "iwp/ifactories.h"
#include "iwp/session.h"

// One running device: its model (UI thread) and its session with all
// endpoints (own worker thread).
class DeviceRuntime : public QObject
{
    Q_OBJECT
public:
    // Takes ownership of the endpoints. configs: role -> endpoint config.
    DeviceRuntime(iwp::IDeviceFactory *device,
                  const QHash<QString, iwp::IEndpoint *> &endpoints,
                  const QHash<QString, QVariantMap> &configs,
                  QObject *parent = nullptr);
    ~DeviceRuntime() override;

    iwp::DeviceModel *model() const { return m_model; }

private:
    QThread m_thread;
    iwp::DeviceModel *m_model = nullptr;
    iwp::SessionBase *m_session = nullptr;
};
