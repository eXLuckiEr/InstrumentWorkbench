#pragma once
#include <QByteArray>
#include <QList>
#include <QUrl>

#include "iwp/endpoint.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QSslError;

// HTTP/HTTPS endpoint (curl replacement) with Redfish-friendly authentication.
//
// Request payload:  "<METHOD> <path>"  or  "<METHOD> <path>\n<json body>"
//   e.g.  "GET /redfish/v1/Chassis"   or   "PATCH /redfish/v1/x\n{\"A\":1}"
// Response: body in `data`, HTTP code in `status`, ok = 2xx.
//
// Config:
//   baseUrl              "https://10.0.0.5" or "http://..."
//   auth                 none | basic | bearer | redfish-session (default)
//   username             optional; empty = ask together with the password
//   credentials          key of the secret in the keychain (default: baseUrl + auth)
//   trustedFingerprint   SHA-256 of the accepted certificate ("AA:BB:..." or hex);
//                        the error text shows the fingerprint of an unknown certificate
//   ignoreSslErrors      true = accept any certificate (like curl -k), not recommended
//   probePath            "/redfish/v1/" (checked after basic/bearer/none auth)
//   sessionPath          "/redfish/v1/SessionService/Sessions"
//   maxConcurrent        parallel requests (default 4)
class HttpEndpoint : public iwp::IEndpoint
{
    Q_OBJECT
public:
    using iwp::IEndpoint::IEndpoint;
    QString kind() const override { return QStringLiteral("http"); }

protected:
    void doOpen(const QVariantMap &config) override;
    void doClose() override;
    void doSend(quint64 id, const iwp::Request &request) override;
    void doAbort(quint64 id) override;

private:
    enum class Auth { None, Basic, Bearer, RedfishSession };
    struct Job {
        quint64 id;
        iwp::Request request;
        bool retried;
    };

    void ensureManager();
    QUrl urlFor(const QString &path) const;
    QNetworkRequest buildRequest(const QUrl &url, bool hasBody, bool withAuth = true) const;
    QNetworkReply *track(QNetworkReply *reply);
    void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    QString describe(QNetworkReply *reply, int status) const;

    // user requests
    void startJob(const Job &job);
    void onJobReply(const Job &job, QNetworkReply *reply);

    // authentication
    void requestCredentials(bool force);
    void onCredentials(const iwp::Credentials &credentials);
    void probe();
    void login(const iwp::Credentials &credentials);
    void loginSucceeded();
    void authFailed();                      // wrong credentials: ask again or give up
    void failAuth(const QString &message);  // give up
    void startRelogin();                    // session expired while running

    QNetworkAccessManager *m_nam = nullptr;
    QString m_baseText;
    QString m_configUser;
    QString m_credKey;
    QString m_probePath;
    QString m_sessionPath;
    QByteArray m_pinned;
    bool m_ignoreSsl = false;
    Auth m_auth = Auth::RedfishSession;

    QByteArray m_authHeader;     // basic / bearer
    QByteArray m_sessionToken;   // redfish
    QString m_sessionPathUri;    // path of our session resource, deleted on close

    QHash<quint64, QNetworkReply *> m_replies;
    QList<Job> m_waiting;        // jobs held back while logging in again
    bool m_loggingIn = false;    // re-login in progress (requests wait)
    bool m_relogin = false;
    int m_loginAttempts = 0;
    QString m_lastUntrusted;     // fingerprint of the last rejected certificate
};
