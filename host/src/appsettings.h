#pragma once
#include <QByteArray>
#include <QList>
#include <QSettings>
#include <QString>
#include <QStringList>

// The only place that knows how and where application settings are stored.
// INI file; the rest of the application uses the typed accessors below, so keys
// are not scattered around the code. UI thread only (sessions get plain values).
//
// File location, first match wins:
//   --settings <file>          explicit file (tests, several setups side by side)
//   <app dir>/iwp.ini          if the file exists: portable mode, e.g. from a USB stick
//   <AppConfigLocation>/settings.ini
//                              Windows %APPDATA%\IWP\..., Linux ~/.config/IWP/...,
//                              macOS ~/Library/Preferences/...
//
// Secrets are NOT stored here (OS keychain, see CredentialStore).
class AppSettings
{
public:
    static constexpr int kVersion = 1;   // layout of the file, see migrate()

    // Call once at startup, after organization and application names are set.
    static void init(const QStringList &arguments);
    static AppSettings &instance();

    QString filePath() const { return m_s.fileName(); }

    QString language() const;
    void setLanguage(const QString &code);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &data);
    QList<int> splitterSizes() const;
    void setSplitterSizes(const QList<int> &sizes);
    QString lastDevice() const;
    void setLastDevice(const QString &deviceId);

    // Connection profile of one endpoint role of one device.
    struct Profile {
        bool exists = false;
        QString choice;   // e.g. "transport/serial"
        QString config;   // JSON text, as shown in the connection row
    };
    Profile profile(const QString &deviceId, const QString &role) const;
    void setProfile(const QString &deviceId, const QString &role, const Profile &profile);

private:
    explicit AppSettings(const QString &path);
    void migrate();
    void importLegacy();

    mutable QSettings m_s;
};
