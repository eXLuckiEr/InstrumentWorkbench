#include "sshendpoint.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QUuid>

namespace {

QString askpassPath()
{
    QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/iwp_askpass");
#ifdef Q_OS_WIN
    path += QStringLiteral(".exe");
#endif
    return QDir::toNativeSeparators(path);
}

} // namespace

SshEndpoint::~SshEndpoint() { stopProcess(); }

// ======================= open / close =======================

void SshEndpoint::doOpen(const QVariantMap &config)
{
    const QString host = config.value("host").toString().trimmed();
    if (host.isEmpty()) {
        fail(tr("No host specified"));
        return;
    }
    const QString helper = askpassPath();
    if (!QFileInfo::exists(helper)) {
        fail(tr("Helper '%1' not found next to the application").arg(helper));
        return;
    }

    m_user = config.value("user").toString().trimmed();
    m_target = m_user.isEmpty() ? host : m_user + QLatin1Char('@') + host;
    const int port = config.value("port", 22).toInt();
    m_credBase = QStringLiteral("ssh|%1:%2").arg(m_target).arg(port);

    QStringList args;
    args << "-T"                                    // no terminal: plain pipes
         << "-o" << "ServerAliveInterval=5"         // a dead link ends the process
         << "-o" << "ServerAliveCountMax=3"
         << "-o" << QStringLiteral("ConnectTimeout=%1").arg(config.value("connectTimeout", 15).toInt())
         << "-o" << "NumberOfPasswordPrompts=3";
    const QString hostKeyChecking = config.value("hostKeyChecking").toString().trimmed();
    if (!hostKeyChecking.isEmpty())
        args << "-o" << "StrictHostKeyChecking=" + hostKeyChecking;
    if (port != 22)
        args << "-p" << QString::number(port);
    const QString identity = config.value("identityFile").toString().trimmed();
    if (!identity.isEmpty())
        args << "-i" << identity;
    const QString jump = config.value("jumpHost").toString().trimmed();
    if (!jump.isEmpty())
        args << "-J" << jump;
    args << config.value("extraArgs").toStringList();
    args << m_target << "sh";

    // channel for the askpass helper
    m_token = QUuid::createUuid().toString(QUuid::Id128).left(12).toLatin1();
    m_askpassSecret = QUuid::createUuid().toString(QUuid::Id128);
    if (!m_server) {
        m_server = new QLocalServer(this);
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
        connect(m_server, &QLocalServer::newConnection, this, &SshEndpoint::onAskpassConnection);
    }
    m_server->close();
    const QString serverName = QStringLiteral("iwp-askpass-") + QString::fromLatin1(m_token);
    QLocalServer::removeServer(serverName);
    if (!m_server->listen(serverName)) {
        fail(tr("Cannot create the askpass channel: %1").arg(m_server->errorString()));
        return;
    }

    startProcess(args, config.value("sshPath", "ssh").toString());
}

void SshEndpoint::startProcess(const QStringList &args, const QString &sshPath)
{
    stopProcess();
    m_out.clear();
    m_err.clear();
    m_ready = false;
    m_closing = false;
    m_promptCount.clear();

    if (!m_process) {
        m_process = new QProcess(this);   // created in the worker thread
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
            m_out += m_process->readAllStandardOutput();
            parseOutput();
        });
        connect(m_process, &QProcess::readyReadStandardError, this, [this] {
            m_err += m_process->readAllStandardError();
            if (m_err.size() > 8192)
                m_err = m_err.right(8192);
        });
        connect(m_process, &QProcess::finished, this,
                [this](int code, QProcess::ExitStatus) { onFinished(code); });
        connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart && !m_closing)
                fail(tr("Cannot start ssh: %1 (install the OpenSSH client or set sshPath)")
                         .arg(m_process->errorString()));
        });
    }

    // OpenSSH asks through our helper instead of a terminal.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SSH_ASKPASS", askpassPath());
    env.insert("SSH_ASKPASS_REQUIRE", "force");       // OpenSSH >= 8.4
    if (!env.contains("DISPLAY"))
        env.insert("DISPLAY", ":0");                  // older versions only use askpass with a DISPLAY
    env.insert("IWP_ASKPASS_SOCKET", m_server->serverName());
    env.insert("IWP_ASKPASS_SECRET", m_askpassSecret);
    m_process->setProcessEnvironment(env);

    m_process->start(sshPath, args);
    // Queued in the pipe until the login is done; the answer tells us the shell is up.
    m_process->write("printf '__IWP_READY_" + m_token + "__\\n'\n");
}

void SshEndpoint::stopProcess()
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;
    m_closing = true;
    m_process->write("exit\n");
    m_process->closeWriteChannel();
    if (!m_process->waitForFinished(1500)) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000))
            m_process->kill();
    }
}

void SshEndpoint::doClose()
{
    stopProcess();
    if (m_server)
        m_server->close();
}

void SshEndpoint::onFinished(int exitCode)
{
    if (m_closing)
        return;
    const QString why = tail(m_err);
    fail(why.isEmpty() ? tr("ssh exited with status %1").arg(exitCode) : tr("ssh: %1").arg(why));
}

QString SshEndpoint::tail(const QByteArray &text)
{
    const QStringList lines = QString::fromLocal8Bit(text).trimmed().split(QLatin1Char('\n'));
    return lines.mid(qMax(0, lines.size() - 3)).join(QStringLiteral(" | ")).trimmed();
}

// ======================= commands =======================

void SshEndpoint::doSend(quint64 id, const iwp::Request &request)
{
    // Single quotes inside the command are escaped, so the command is data for
    // `eval` in a subshell: a syntax error in it cannot kill the session, and
    // it cannot read our following commands from stdin.
    QByteArray quoted = request.payload;
    quoted.replace("'", "'\\''");
    QByteArray script = "( eval '" + quoted + "' ) </dev/null 2>&1\n";
    if (request.expectResponse)
        script += "printf '\\n__IWP_" + m_token + "_" + QByteArray::number(id) + "__ %d\\n' $?\n";
    m_process->write(script);

    if (!request.expectResponse) {
        iwp::Response r;
        r.ok = true;
        finish(id, r);
    }
}

void SshEndpoint::parseOutput()
{
    if (!m_ready) {
        const QByteArray ready = "__IWP_READY_" + m_token + "__\n";
        const int idx = m_out.indexOf(ready);
        if (idx < 0)
            return;
        m_out.remove(0, idx + ready.size());
        m_ready = true;
        setState(State::Online);
    }

    // Each answer ends with "\n__IWP_<token>_<id>__ <exit status>\n".
    const QByteArray prefix = "\n__IWP_" + m_token + "_";
    int from = 0;
    while (true) {
        const int s = m_out.indexOf(prefix, from);
        if (s < 0)
            return;
        int i = s + prefix.size();
        int j = i;
        while (j < m_out.size() && m_out.at(j) >= '0' && m_out.at(j) <= '9')
            ++j;
        if (j + 3 > m_out.size())
            return;                                   // marker not complete yet
        if (m_out.mid(j, 3) != "__ ") {
            from = s + 1;                             // not our marker
            continue;
        }
        int k = j + 3;
        int e = k;
        while (e < m_out.size() && m_out.at(e) >= '0' && m_out.at(e) <= '9')
            ++e;
        if (e >= m_out.size())
            return;                                   // newline not there yet
        if (e == k || m_out.at(e) != '\n') {
            from = s + 1;
            continue;
        }

        const quint64 id = m_out.mid(i, j - i).toULongLong();
        const int exitStatus = m_out.mid(k, e - k).toInt();
        iwp::Response r;
        r.data = m_out.left(s);
        r.status = exitStatus;
        r.ok = (exitStatus == 0);
        if (!r.ok)
            r.error = tr("Command exited with status %1").arg(exitStatus);
        m_out.remove(0, e + 1);
        from = 0;
        finish(id, r);   // ignored if the request has timed out meanwhile
    }
}

// ======================= questions from OpenSSH =======================

void SshEndpoint::onAskpassConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            if (socket->property("handled").toBool() || !socket->canReadLine())
                return;
            socket->setProperty("handled", true);
            handleAskpass(socket, QJsonDocument::fromJson(socket->readLine()).object());
        });
    }
}

SshEndpoint::PromptInfo SshEndpoint::classify(const QString &prompt) const
{
    PromptInfo info;
    const QString p = prompt.toLower();
    static const QRegularExpression pin(QStringLiteral("\\bpin\\b"));
    static const QRegularExpression quoted(QStringLiteral("'([^']*)'"));

    if (p.contains(QLatin1String("passphrase"))) {
        const auto m = quoted.match(prompt);
        info.key = m_credBase + QStringLiteral("|passphrase|") + (m.hasMatch() ? m.captured(1) : QString());
        info.label = tr("Passphrase");
    } else if (p.contains(pin)) {
        info.key = m_credBase + QStringLiteral("|pin");
        info.label = tr("PIN");
        info.remember = false;                        // would defeat the second factor
    } else if (p.contains(QLatin1String("password"))) {
        info.key = m_credBase + QStringLiteral("|password");
        info.label = tr("Password");
    } else {                                          // verification code, OTP...
        info.key = m_credBase + QStringLiteral("|other");
        info.label = tr("Response");
        info.remember = false;
    }
    return info;
}

void SshEndpoint::handleAskpass(QLocalSocket *socket, const QJsonObject &request)
{
    if (request.value("secret").toString() != m_askpassSecret) {
        socket->disconnectFromServer();               // not our helper
        return;
    }
    const QString prompt = request.value("prompt").toString();
    const QString type = request.value("type").toString();   // "", "confirm", "none"

    const QPointer<QLocalSocket> guard(socket);
    auto reply = [this, guard](bool ok, const QString &mode, const QString &text) {
        if (state() == State::Authenticating)
            setState(State::Connecting);
        if (!guard)
            return;                                   // the helper has gone
        QJsonObject o;
        o["ok"] = ok;
        o["mode"] = mode;
        o["text"] = text;
        guard->write(QJsonDocument(o).toJson(QJsonDocument::Compact) + "\n");
        guard->flush();
        guard->disconnectFromServer();
    };

    if (type == QLatin1String("none")) {              // e.g. "Confirm user presence": touch the key
        emit userActionRequired(prompt);
        reply(true, QStringLiteral("info"), QString());
        return;
    }
    if (!credentials()) {
        reply(false, QStringLiteral("secret"), QString());
        return;
    }
    if (state() == State::Connecting)
        setState(State::Authenticating);

    if (type == QLatin1String("confirm") || prompt.contains(QLatin1String("(yes/no"))) {
        const bool exitStyle = (type == QLatin1String("confirm"));
        credentials()->confirm(tr("SSH: %1").arg(m_target), prompt, this, [reply, exitStyle](bool yes) {
            if (exitStyle)
                reply(yes, QStringLiteral("confirm-exit"), QString());
            else   // older OpenSSH: the answer is typed text
                reply(true, QStringLiteral("confirm-text"), yes ? QStringLiteral("yes") : QStringLiteral("no"));
        });
        return;
    }

    const PromptInfo info = classify(prompt);
    iwp::CredentialRequest req;
    req.key = info.key;
    req.title = tr("SSH: %1").arg(m_target);
    req.message = prompt;
    req.username = m_user;
    req.askUsername = false;
    req.secretLabel = info.label;
    req.allowRemember = info.remember;
    req.forcePrompt = (++m_promptCount[info.key] > 1);   // asked again: the last answer was wrong
    credentials()->get(req, this, [reply](const iwp::Credentials &c) {
        reply(c.ok, QStringLiteral("secret"), c.secret);
    });
}
