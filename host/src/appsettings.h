#pragma once
#include <QByteArray>
#include <QList>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <optional>

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

    // Graphs (see docs/plot.md): frames per second (default 60; lower it on weak hardware) and
    // the renderer: "auto" (GPU where it is useful), "gpu" or "cpu".
    int plotFrameRate() const;
    QString plotRenderer() const;

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &data);
    QList<int> splitterSizes() const;
    void setSplitterSizes(const QList<int> &sizes);
    QString lastDevice() const;
    void setLastDevice(const QString &deviceId);

    // Connection profile of one endpoint role of one device: which way was chosen
    // (e.g. "transport/serial") and, separately for each way, its settings as JSON text,
    // so that switching between serial and TCP does not lose what was typed.
    std::optional<QString> choice(const QString &deviceId, const QString &role) const;   // empty: never saved
    void setChoice(const QString &deviceId, const QString &role, const QString &choice);
    QString config(const QString &deviceId, const QString &role, const QString &choice) const;
    void setConfig(const QString &deviceId, const QString &role, const QString &choice,
                   const QString &json);

private:
    explicit AppSettings(const QString &path);
    void migrate();
    void importLegacy();

    mutable QSettings m_s;
};
