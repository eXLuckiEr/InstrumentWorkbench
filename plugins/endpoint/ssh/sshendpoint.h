#pragma once
#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QProcess>

#include "iwp/endpoint.h"

class QLocalServer;
class QLocalSocket;

// Command channel over SSH. One long-lived "ssh host sh" process; every
// request is a shell command whose output (stdout+stderr) is the response.
//
//   payload   shell command (may be several lines). Each request runs in its
//             own subshell: nothing (cd, variables) carries over.
//   data      output of the command
//   status    exit status of the command; ok = (status == 0)
//
// Authentication is left to the system OpenSSH (keys, ssh-agent, FIDO2/YubiKey,
// certificates, ProxyJump, ~/.ssh/config). Whatever it asks (password, key
// passphrase, PIN, host key confirmation, "touch your key") is caught through
// SSH_ASKPASS and shown by the host; passwords/passphrases may be kept in the
// OS keychain, PINs and one-time codes never.
//
// Config: host, port, user, identityFile, jumpHost, hostKeyChecking,
//         connectTimeout, sshPath, extraArgs (list of ssh arguments)
class SshEndpoint : public iwp::IEndpoint
{
    Q_OBJECT
public:
    using iwp::IEndpoint::IEndpoint;
    ~SshEndpoint() override;
    QString kind() const override { return QStringLiteral("ssh"); }

protected:
    void doOpen(const QVariantMap &config) override;
    void doClose() override;
    void doSend(quint64 id, const iwp::Request &request) override;

private:
    struct PromptInfo {
        QString key;
        QString label;
        bool remember = true;
    };

    void startProcess(const QStringList &args, const QString &sshPath);
    void stopProcess();
    void parseOutput();
    void onFinished(int exitCode);
    void onAskpassConnection();
    void handleAskpass(QLocalSocket *socket, const QJsonObject &request);
    PromptInfo classify(const QString &prompt) const;
    static QString tail(const QByteArray &text);

    QProcess *m_process = nullptr;
    QLocalServer *m_server = nullptr;
    QByteArray m_out;
    QByteArray m_err;
    QByteArray m_token;         // marks command boundaries in the output
    QString m_askpassSecret;    // only our own askpass helper may talk to us
    bool m_ready = false;
    bool m_closing = false;

    QString m_target;           // user@host
    QString m_credBase;
    QString m_user;
    QHash<QString, int> m_promptCount;
};
