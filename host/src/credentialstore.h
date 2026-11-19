#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>

#include "iwp/credentials.h"

class QWidget;

// Asks the user for credentials and keeps them in the OS keychain
// (macOS Keychain, Windows Credential Manager, libsecret) through QtKeychain.
// Without QtKeychain nothing is saved: the user is asked every time.
// Lives in the UI thread; get()/forget() may be called from any thread.
class CredentialStore : public QObject, public iwp::ICredentialProvider
{
    Q_OBJECT
public:
    explicit CredentialStore(QWidget *dialogParent, QObject *parent = nullptr);

    static bool keychainAvailable();

    void get(const iwp::CredentialRequest &request, QObject *context,
             std::function<void(const iwp::Credentials &)> done) override;
    void confirm(const QString &title, const QString &message, QObject *context,
                 std::function<void(bool)> done) override;
    void forget(const QString &key) override;

private:
    struct Waiter {
        QPointer<QObject> context;
        std::function<void(const iwp::Credentials &)> done;
    };
    using StoredCallback = std::function<void(bool found, const QString &user, const QString &secret)>;

    void handleGet(const iwp::CredentialRequest &request, const Waiter &waiter);
    void prompt(const iwp::CredentialRequest &request, const QString &storedUser);
    void deliver(const QString &key, const iwp::Credentials &credentials);
    void readStored(const QString &key, StoredCallback callback);
    void writeStored(const QString &key, const QString &user, const QString &secret);

    QWidget *m_dialogParent;
    QHash<QString, QList<Waiter>> m_inFlight;   // one dialog per key
};
