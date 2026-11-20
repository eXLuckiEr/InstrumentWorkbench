#include "deviceruntime.h"

#include <QEventLoop>
#include <QTimer>

DeviceRuntime::DeviceRuntime(iwp::IDeviceFactory *device,
                             const QHash<QString, iwp::IEndpoint *> &endpoints,
                             const QHash<QString, QVariantMap> &configs,
                             QObject *parent)
    : QObject(parent)
{
    m_model = new iwp::DeviceModel(this);               // stays in the UI thread
    m_session = device->createSession(endpoints, m_model);

    connect(&m_thread, &QThread::finished, m_session, &QObject::deleteLater);
    m_session->moveToThread(&m_thread);                 // endpoints move with it
    m_thread.start();

    iwp::SessionBase *session = m_session;
    QMetaObject::invokeMethod(session, [session, endpoints, configs] {
        session->start();                               // subscribe first...
        for (auto it = configs.cbegin(); it != configs.cend(); ++it)
            if (iwp::IEndpoint *ep = endpoints.value(it.key(), nullptr))
                ep->open(it.value());                   // ...then open
    }, Qt::QueuedConnection);
}

DeviceRuntime::~DeviceRuntime()
{
    // stop() is graceful (the session may still talk to the device), so wait for its
    // `stopped` signal; the timeout only guards against a stuck worker thread.
    iwp::SessionBase *session = m_session;
    QEventLoop loop;
    connect(session, &iwp::SessionBase::stopped, &loop, &QEventLoop::quit);
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QMetaObject::invokeMethod(session, [session] { session->stop(); }, Qt::QueuedConnection);
    loop.exec();
    m_thread.quit();
    m_thread.wait();    // the session is deleted by QThread::finished -> deleteLater
}
