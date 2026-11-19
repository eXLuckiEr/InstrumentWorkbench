#pragma once
#include <QObject>
#include <QString>
#include <functional>

#include "iwp/global.h"

namespace iwp {

struct CredentialRequest {
    QString key;                    // stable id of the secret, e.g. "https://10.0.0.5|redfish-session"
    QString title;                  // dialog title
    QString message;                // text shown above the fields
    QString username;               // preset user name
    QString secretLabel;            // "Password", "Token"
    bool askUsername = true;        // false: the user name is fixed (or not needed)
    bool forcePrompt = false;       // true: the stored secret was rejected, ask again
    bool allowRemember = true;      // false for one-time codes and PINs: never offered for saving
};

struct Credentials {
    bool ok = false;                // false: the user cancelled / nothing available
    QString username;
    QString secret;
};

// Provided by the host. Plugins never store secrets themselves: the host asks
// the user and keeps them in the OS keychain (Keychain / Credential Manager /
// libsecret) when it is available.
class IWP_SDK_EXPORT ICredentialProvider
{
public:
    virtual ~ICredentialProvider() = default;

    // Thread safe. `done` is invoked later in the thread of `context`
    // (an object owned by the caller); if `context` dies first, it is not called.
    virtual void get(const CredentialRequest &request, QObject *context,
                     std::function<void(const Credentials &)> done) = 0;

    // Yes/no question, e.g. "Trust this SSH host key?". Thread safe, same rules as get().
    virtual void confirm(const QString &title, const QString &message, QObject *context,
                         std::function<void(bool)> done) = 0;

    // Forget a stored secret.
    virtual void forget(const QString &key) = 0;
};

} // namespace iwp
