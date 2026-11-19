#include "httpendpoint.h"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QTimer>

namespace {

QByteArray normalizeFingerprint(QString text)
{
    text.remove(QLatin1Char(':')).remove(QLatin1Char(' ')).remove(QLatin1Char('-'));
    return text.toLatin1().toUpper();
}

QString withColons(const QByteArray &hex)
{
    QStringList parts;
    for (int i = 0; i + 1 < hex.size(); i += 2)
        parts << QString::fromLatin1(hex.mid(i, 2));
    return parts.join(QLatin1Char(':'));
}

int httpStatus(const QNetworkReply *reply)
{
    return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

} // namespace

// ======================= open / close =======================

void HttpEndpoint::doOpen(const QVariantMap &config)
{
    const QString base = config.value("baseUrl").toString().trimmed();
    const QUrl url(base);
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        fail(tr("Bad or missing baseUrl: '%1'").arg(base));
        return;
    }
    m_baseText = url.toString(QUrl::RemoveUserInfo | QUrl::StripTrailingSlash);

    const QString auth = config.value("auth", "redfish-session").toString().toLower();
    if (auth == QLatin1String("none"))                  m_auth = Auth::None;
    else if (auth == QLatin1String("basic"))            m_auth = Auth::Basic;
    else if (auth == QLatin1String("bearer"))           m_auth = Auth::Bearer;
    else if (auth == QLatin1String("redfish-session"))  m_auth = Auth::RedfishSession;
    else {
        fail(tr("Unknown auth '%1'").arg(auth));
        return;
    }

    m_configUser = config.value("username").toString();
    m_credKey = config.value("credentials").toString();
    if (m_credKey.isEmpty())
        m_credKey = m_baseText + QLatin1Char('|') + auth;
    m_probePath = config.value("probePath", "/redfish/v1/").toString();
    m_sessionPath = config.value("sessionPath", "/redfish/v1/SessionService/Sessions").toString();
    m_pinned = normalizeFingerprint(config.value("trustedFingerprint").toString());
    m_ignoreSsl = config.value("ignoreSslErrors", false).toBool();
    setMaxConcurrent(config.value("maxConcurrent", 4).toInt());

    ensureManager();
    m_loginAttempts = 0;
    m_loggingIn = false;
    m_relogin = false;
    m_waiting.clear();
    m_authHeader.clear();
    m_sessionToken.clear();
    m_sessionPathUri.clear();
    m_lastUntrusted.clear();

    if (m_auth == Auth::None)
        probe();
    else
        requestCredentials(false);
}

void HttpEndpoint::doClose()
{
    for (auto it = m_replies.begin(); it != m_replies.end(); ++it) {
        it.value()->disconnect(this);
        it.value()->abort();
        it.value()->deleteLater();
    }
    m_replies.clear();
    m_waiting.clear();
    m_loggingIn = false;
    m_relogin = false;

    // Free the BMC session slot (they are few): wait for the answer, but not long.
    if (m_nam && m_auth == Auth::RedfishSession && !m_sessionToken.isEmpty()
        && !m_sessionPathUri.isEmpty()) {
        QNetworkReply *reply = track(m_nam->deleteResource(
            buildRequest(urlFor(m_sessionPathUri), false)));
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        loop.exec();
        reply->disconnect(this);
        if (!reply->isFinished())
            reply->abort();
        reply->deleteLater();
    }
    m_sessionToken.clear();
    m_sessionPathUri.clear();
    m_authHeader.clear();
}

void HttpEndpoint::ensureManager()
{
    if (m_nam)
        return;
    m_nam = new QNetworkAccessManager(this);                 // created in the worker thread
    m_nam->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));  // BMCs are on the local network
}

// ======================= helpers =======================

QUrl HttpEndpoint::urlFor(const QString &path) const
{
    return QUrl(m_baseText + (path.startsWith(QLatin1Char('/')) ? path : QLatin1Char('/') + path));
}

QNetworkRequest HttpEndpoint::buildRequest(const QUrl &url, bool hasBody, bool withAuth) const
{
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("OData-Version", "4.0");
    if (hasBody)
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (withAuth) {
        if (!m_authHeader.isEmpty())
            req.setRawHeader("Authorization", m_authHeader);
        if (!m_sessionToken.isEmpty())
            req.setRawHeader("X-Auth-Token", m_sessionToken);
    }
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    req.setTransferTimeout(30000);
    return req;
}

QNetworkReply *HttpEndpoint::track(QNetworkReply *reply)
{
    connect(reply, &QNetworkReply::sslErrors, this,
            [this, reply](const QList<QSslError> &errors) { onSslErrors(reply, errors); });
    return reply;
}

void HttpEndpoint::onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    if (m_ignoreSsl) {
        reply->ignoreSslErrors();
        return;
    }
    const QSslCertificate cert = reply->sslConfiguration().peerCertificate();
    const QByteArray fp = cert.digest(QCryptographicHash::Sha256).toHex().toUpper();
    if (!m_pinned.isEmpty() && fp == m_pinned) {
        reply->ignoreSslErrors(errors);   // pinned certificate: self-signed/expired is fine
        return;
    }
    m_lastUntrusted = withColons(fp);
}

QString HttpEndpoint::describe(QNetworkReply *reply, int status) const
{
    if (reply->error() == QNetworkReply::SslHandshakeFailedError && !m_lastUntrusted.isEmpty())
        return tr("TLS certificate is not trusted (SHA-256 %1). Put it into \"trustedFingerprint\" "
                  "to accept it, or use http://").arg(m_lastUntrusted);
    if (status > 0)
        return tr("HTTP %1 %2")
            .arg(status)
            .arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
    return reply->errorString();
}

// ======================= user requests =======================

void HttpEndpoint::doSend(quint64 id, const iwp::Request &request)
{
    startJob(Job{id, request, false});
}

void HttpEndpoint::doAbort(quint64 id)
{
    if (QNetworkReply *reply = m_replies.take(id)) {
        reply->disconnect(this);   // so that the abort is reported as a timeout, not as an error
        reply->abort();
        reply->deleteLater();
    }
    m_waiting.removeIf([id](const Job &j) { return j.id == id; });
}

void HttpEndpoint::startJob(const Job &job)
{
    if (m_loggingIn) {          // wait until the new session is there
        m_waiting.append(job);
        return;
    }

    const QByteArray &payload = job.request.payload;
    const int nl = payload.indexOf('\n');
    const QByteArray head = (nl >= 0 ? payload.left(nl) : payload).trimmed();
    const QByteArray body = nl >= 0 ? payload.mid(nl + 1) : QByteArray();
    const int sp = head.indexOf(' ');
    const QByteArray method = (sp > 0 ? head.left(sp) : QByteArray("GET")).toUpper();
    const QString path = QString::fromUtf8(sp > 0 ? head.mid(sp + 1).trimmed() : head);

    if (!path.startsWith(QLatin1Char('/'))) {   // never send credentials to another host
        iwp::Response r;
        r.error = tr("Path must start with '/': '%1'").arg(path);
        finish(job.id, r);
        return;
    }

    const QNetworkRequest req = buildRequest(urlFor(path), !body.isEmpty());
    QNetworkReply *reply = nullptr;
    if (method == "GET")
        reply = m_nam->get(req);
    else
        reply = m_nam->sendCustomRequest(req, method, body);

    track(reply);
    m_replies.insert(job.id, reply);
    connect(reply, &QNetworkReply::finished, this, [this, job, reply] { onJobReply(job, reply); });
}

void HttpEndpoint::onJobReply(const Job &job, QNetworkReply *reply)
{
    m_replies.remove(job.id);
    reply->deleteLater();
    const int status = httpStatus(reply);

    if (status == 401 && m_auth == Auth::RedfishSession && !job.retried) {
        Job again = job;                 // the session probably expired: log in and retry once
        again.retried = true;
        m_waiting.append(again);
        if (!m_loggingIn)
            startRelogin();
        return;
    }

    iwp::Response r;
    r.status = status;
    r.data = reply->readAll();
    r.ok = (reply->error() == QNetworkReply::NoError) && status >= 200 && status < 300;
    if (!r.ok)
        r.error = describe(reply, status);
    finish(job.id, r);
}

// ======================= authentication =======================

void HttpEndpoint::requestCredentials(bool force)
{
    if (!credentials()) {
        failAuth(tr("No credential provider available"));
        return;
    }
    if (!m_relogin)
        setState(State::Authenticating);

    iwp::CredentialRequest req;
    req.key = m_credKey;
    req.title = tr("Credentials for %1").arg(QUrl(m_baseText).host());
    req.message = force ? tr("The credentials for %1 were rejected. Enter them again.").arg(m_baseText)
                        : tr("Enter the credentials for %1").arg(m_baseText);
    req.username = m_configUser;
    req.askUsername = (m_auth != Auth::Bearer) && m_configUser.isEmpty();
    req.secretLabel = (m_auth == Auth::Bearer) ? tr("Token") : tr("Password");
    req.forcePrompt = force;
    credentials()->get(req, this, [this](const iwp::Credentials &c) { onCredentials(c); });
}

void HttpEndpoint::onCredentials(const iwp::Credentials &c)
{
    if (!c.ok) {
        failAuth(tr("Credentials were not provided"));
        return;
    }
    switch (m_auth) {
    case Auth::Basic:
        m_authHeader = "Basic " + (c.username + QLatin1Char(':') + c.secret).toUtf8().toBase64();
        probe();
        break;
    case Auth::Bearer:
        m_authHeader = "Bearer " + c.secret.toUtf8();
        probe();
        break;
    case Auth::RedfishSession:
        login(c);
        break;
    case Auth::None:
        probe();
        break;
    }
}

void HttpEndpoint::probe()
{
    setState(State::Connecting);
    QNetworkReply *reply = track(m_nam->get(buildRequest(urlFor(m_probePath), false)));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int status = httpStatus(reply);
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
        const QString error = describe(reply, status);
        reply->deleteLater();
        if (ok)
            setState(State::Online);
        else if ((status == 401 || status == 403) && m_auth != Auth::None)
            authFailed();
        else
            failAuth(error);
    });
}

void HttpEndpoint::login(const iwp::Credentials &c)
{
    QJsonObject body;
    body["UserName"] = c.username;
    body["Password"] = c.secret;
    QNetworkReply *reply = track(m_nam->post(
        buildRequest(urlFor(m_sessionPath), true, false),
        QJsonDocument(body).toJson(QJsonDocument::Compact)));

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int status = httpStatus(reply);
        const QByteArray token = reply->rawHeader("X-Auth-Token");
        const QByteArray location = reply->rawHeader("Location");
        const bool ok = reply->error() == QNetworkReply::NoError
                        && (status == 200 || status == 201) && !token.isEmpty();
        const QString error = describe(reply, status);
        reply->deleteLater();

        if (ok) {
            m_sessionToken = token;
            // keep only the path: the BMC may report an internal address in the URL
            m_sessionPathUri = QUrl(QString::fromUtf8(location)).path();
            loginSucceeded();
        } else if (status == 401 || status == 403) {
            authFailed();
        } else {
            failAuth(error.isEmpty() ? tr("Login failed") : error);
        }
    });
}

void HttpEndpoint::loginSucceeded()
{
    m_loginAttempts = 0;
    m_loggingIn = false;
    m_relogin = false;
    if (state() != State::Online)
        setState(State::Online);

    const QList<Job> jobs = m_waiting;
    m_waiting.clear();
    for (const Job &j : jobs)
        startJob(j);
}

void HttpEndpoint::authFailed()
{
    if (++m_loginAttempts >= 3) {
        failAuth(tr("Authentication failed"));
        return;
    }
    requestCredentials(true);   // ask again, the stored ones are wrong
}

void HttpEndpoint::failAuth(const QString &message)
{
    if (m_loggingIn) {          // running session: give the waiting requests an answer
        m_loggingIn = false;
        m_relogin = false;
        iwp::Response r;
        r.error = message;
        const QList<Job> jobs = m_waiting;
        m_waiting.clear();
        for (const Job &j : jobs)
            finish(j.id, r);
    }
    fail(message);
}

void HttpEndpoint::startRelogin()
{
    m_loggingIn = true;
    m_relogin = true;
    m_loginAttempts = 0;
    m_sessionToken.clear();
    requestCredentials(false);
}
