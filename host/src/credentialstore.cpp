#include "credentialstore.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>

#ifdef IWP_HAVE_QTKEYCHAIN
#  if __has_include(<qt6keychain/keychain.h>)
#    include <qt6keychain/keychain.h>
#  else
#    include <keychain.h>
#  endif
static const QString kService = QStringLiteral("InstrumentWorkbench");
#endif

CredentialStore::CredentialStore(QWidget *dialogParent, QObject *parent)
    : QObject(parent), m_dialogParent(dialogParent)
{
}

bool CredentialStore::keychainAvailable()
{
#ifdef IWP_HAVE_QTKEYCHAIN
    return true;
#else
    return false;
#endif
}

// ---------------- public, thread safe ----------------

void CredentialStore::get(const iwp::CredentialRequest &request, QObject *context,
                          std::function<void(const iwp::Credentials &)> done)
{
    const Waiter waiter{QPointer<QObject>(context), std::move(done)};
    QMetaObject::invokeMethod(this, [this, request, waiter] { handleGet(request, waiter); },
                              Qt::QueuedConnection);
}

void CredentialStore::confirm(const QString &title, const QString &message, QObject *context,
                              std::function<void(bool)> done)
{
    const QPointer<QObject> ctx(context);
    QMetaObject::invokeMethod(this, [this, title, message, ctx, done] {
        auto *box = new QMessageBox(QMessageBox::Question, title, message,
                                    QMessageBox::Yes | QMessageBox::No, m_dialogParent);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setDefaultButton(QMessageBox::No);   // safe default
        connect(box, &QDialog::finished, this, [ctx, done](int result) {
            if (!ctx)
                return;
            const bool yes = (result == QMessageBox::Yes);
            QMetaObject::invokeMethod(ctx.data(), [done, yes] { done(yes); }, Qt::QueuedConnection);
        });
        box->open();
    }, Qt::QueuedConnection);
}

void CredentialStore::forget(const QString &key)
{
#ifdef IWP_HAVE_QTKEYCHAIN
    QMetaObject::invokeMethod(this, [this, key] {
        auto *job = new QKeychain::DeletePasswordJob(kService, this);
        job->setAutoDelete(true);
        job->setKey(QStringLiteral("cred:") + key);
        job->start();
    }, Qt::QueuedConnection);
#else
    Q_UNUSED(key)
#endif
}

// ---------------- UI thread ----------------

void CredentialStore::handleGet(const iwp::CredentialRequest &request, const Waiter &waiter)
{
    QList<Waiter> &waiters = m_inFlight[request.key];
    waiters.append(waiter);
    if (waiters.size() > 1)
        return;   // a request for this key is already being handled

    if (!keychainAvailable()) {
        prompt(request, QString());
        return;
    }
    readStored(request.key, [this, request](bool found, const QString &user, const QString &secret) {
        if (found && !request.forcePrompt) {
            iwp::Credentials c;
            c.ok = true;
            c.username = user;
            c.secret = secret;
            deliver(request.key, c);
        } else {
            prompt(request, found ? user : QString());   // rejected before: prefill the user name
        }
    });
}

void CredentialStore::prompt(const iwp::CredentialRequest &request, const QString &storedUser)
{
    auto *dialog = new QDialog(m_dialogParent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(request.title.isEmpty() ? tr("Credentials") : request.title);

    auto *form = new QFormLayout(dialog);
    if (!request.message.isEmpty())
        form->addRow(new QLabel(request.message));

    auto *user = new QLineEdit(request.username.isEmpty() ? storedUser : request.username);
    if (request.askUsername)
        form->addRow(tr("User name"), user);

    auto *secret = new QLineEdit;
    secret->setEchoMode(QLineEdit::Password);
    form->addRow(request.secretLabel.isEmpty() ? tr("Password") : request.secretLabel, secret);

    auto *remember = new QCheckBox(tr("Remember in the system keychain"));
    const bool canRemember = keychainAvailable() && request.allowRemember;
    remember->setChecked(canRemember);
    remember->setEnabled(canRemember);
    remember->setVisible(request.allowRemember);   // one-time codes / PINs: no such option
    form->addRow(remember);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    connect(dialog, &QDialog::finished, this, [=](int result) {
        iwp::Credentials c;
        if (result == QDialog::Accepted) {
            c.ok = true;
            c.username = request.askUsername ? user->text() : request.username;
            c.secret = secret->text();
            if (remember->isChecked() && keychainAvailable() && request.allowRemember)
                writeStored(request.key, c.username, c.secret);
        }
        deliver(request.key, c);
    });

    if (request.askUsername && user->text().isEmpty())
        user->setFocus();
    else
        secret->setFocus();
    dialog->open();
}

void CredentialStore::deliver(const QString &key, const iwp::Credentials &credentials)
{
    const QList<Waiter> waiters = m_inFlight.take(key);
    for (const Waiter &w : waiters) {
        if (!w.context)
            continue;   // the endpoint is gone
        const auto done = w.done;
        QMetaObject::invokeMethod(w.context.data(), [done, credentials] { done(credentials); },
                                  Qt::QueuedConnection);
    }
}

// ---------------- keychain ----------------

void CredentialStore::readStored(const QString &key, StoredCallback callback)
{
#ifdef IWP_HAVE_QTKEYCHAIN
    auto *job = new QKeychain::ReadPasswordJob(kService, this);
    job->setAutoDelete(true);
    job->setKey(QStringLiteral("cred:") + key);
    connect(job, &QKeychain::Job::finished, this, [job, callback](QKeychain::Job *) {
        if (job->error() != QKeychain::NoError) {   // not found, denied, no backend...
            callback(false, QString(), QString());
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(job->textData().toUtf8()).object();
        callback(o.contains("p"), o.value("u").toString(), o.value("p").toString());
    });
    job->start();
#else
    Q_UNUSED(key)
    callback(false, QString(), QString());
#endif
}

void CredentialStore::writeStored(const QString &key, const QString &user, const QString &secret)
{
#ifdef IWP_HAVE_QTKEYCHAIN
    QJsonObject o;
    o["u"] = user;
    o["p"] = secret;
    auto *job = new QKeychain::WritePasswordJob(kService, this);
    job->setAutoDelete(true);
    job->setKey(QStringLiteral("cred:") + key);
    job->setTextData(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    job->start();
#else
    Q_UNUSED(key) Q_UNUSED(user) Q_UNUSED(secret)
#endif
}
