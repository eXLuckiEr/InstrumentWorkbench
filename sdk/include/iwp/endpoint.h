#pragma once
#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <functional>

#include "iwp/credentials.h"
#include "iwp/global.h"

class QTimer;

namespace iwp {

struct Request {
    QByteArray payload;        // SCPI line, "GET /path", shell command...
    int timeoutMs = 1000;
    int priority = 0;          // higher goes first; user commands > polling
    bool expectResponse = true;
};

struct Response {
    bool ok = false;
    QByteArray data;
    QString error;
    qint64 latencyMs = 0;
    int status = 0;            // protocol status if there is one (HTTP code); 0 = no answer at all

    QString text() const { return QString::fromUtf8(data).trimmed(); }
    double toDouble(bool *okOut = nullptr) const { return text().toDouble(okOut); }
};

using ResponseHandler = std::function<void(const Response &)>;

// A place where requests can be sent and answers received: SCPI over a
// serial port, VISA resource, HTTP API, SSH shell...
//
// The base class owns the request queue: priorities, timeouts, limited
// concurrency, failing everything when the endpoint goes offline.
// Implementations only deliver a request (doSend) and report the answer
// with finish().
//
// All methods must be used from the thread the endpoint lives in.
class IWP_SDK_EXPORT IEndpoint : public QObject
{
    Q_OBJECT
public:
    enum class State { Offline, Connecting, Authenticating, Online, Error };
    Q_ENUM(State)

    explicit IEndpoint(QObject *parent = nullptr);
    ~IEndpoint() override;

    State state() const { return m_state; }
    // The config the endpoint was opened with (empty before the first open()).
    QVariantMap config() const { return m_config; }
    virtual QString kind() const = 0;      // "scpi", "stream", "http", "ssh"...

    // Set by the host before the endpoint is opened (any thread, before use).
    void setCredentialProvider(ICredentialProvider *provider) { m_credentials = provider; }

    // The handler is always called exactly once (also on timeout/offline).
    void submit(const Request &request, ResponseHandler handler);

public slots:
    void open(const QVariantMap &config);
    void close();

signals:
    void stateChanged(iwp::IEndpoint::State state);
    void errorOccurred(const QString &message);
    // e.g. "Touch your security key", "Enter PIN"
    void userActionRequired(const QString &message);
    // data nobody asked for: raw stream, push events, CAN frames
    void unsolicited(const QByteArray &data);

protected:
    virtual void doOpen(const QVariantMap &config) = 0;
    virtual void doClose() = 0;
    // Start executing the request; call finish(id, ...) when it is done.
    virtual void doSend(quint64 id, const Request &request) = 0;
    // The request timed out: drop any state belonging to it.
    virtual void doAbort(quint64 id) { Q_UNUSED(id) }

    ICredentialProvider *credentials() const { return m_credentials; }

    void finish(quint64 id, Response response);
    void setState(State state);
    void fail(const QString &message);     // errorOccurred + State::Error
    void setMaxConcurrent(int n);          // default 1 (SCPI is strictly serial)

private:
    struct Pending {
        quint64 id;
        Request request;
        ResponseHandler handler;
    };
    struct InFlight {
        ResponseHandler handler;
        qint64 startedMs;
        QTimer *timer;
    };
    void pump();
    void failAll(const QString &reason);

    State m_state = State::Offline;
    QVariantMap m_config;
    QList<Pending> m_queue;
    QHash<quint64, InFlight> m_inflight;
    quint64 m_nextId = 1;
    int m_maxConcurrent = 1;
    QElapsedTimer m_clock;
    ICredentialProvider *m_credentials = nullptr;
};

} // namespace iwp
