#pragma once
#include <QByteArray>
#include <QObject>
#include <QVariantMap>

#include "iwp/global.h"

namespace iwp {

// Physical channel (TCP, UART, CAN, VISA...). Moves bytes, knows nothing
// about protocols or devices.
//
// Threading: the host creates a transport via ITransportFactory::create()
// on the main thread and then moves it to a worker thread. Therefore:
//   * do NOT create QObject children (sockets, timers, ports) in the
//     constructor - create them lazily in doOpen();
//   * open()/close()/write() are slots; call them through queued
//     connections or QMetaObject::invokeMethod(..., Qt::QueuedConnection).
class IWP_SDK_EXPORT ITransport : public QObject
{
    Q_OBJECT
public:
    enum class State { Closed, Opening, Open, Error };
    Q_ENUM(State)

    explicit ITransport(QObject *parent = nullptr);
    ~ITransport() override;

    State state() const { return m_state; }
    virtual QString typeId() const = 0;

public slots:
    void open(const QVariantMap &config);
    void close();
    void write(const QByteArray &data);

signals:
    void stateChanged(iwp::ITransport::State state);
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &message);

protected:
    // Start opening. Call setState(State::Open) when really open (may be
    // asynchronous) or fail() on error.
    virtual void doOpen(const QVariantMap &config) = 0;
    virtual void doClose() = 0;
    virtual qint64 doWrite(const QByteArray &data) = 0;

    void setState(State state);
    void fail(const QString &message);   // errorOccurred + State::Error

private:
    State m_state = State::Closed;
};

} // namespace iwp
