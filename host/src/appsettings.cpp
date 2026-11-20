#include "appsettings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

AppSettings *g_instance = nullptr;

QString settingsPath(const QStringList &arguments)
{
    const int i = arguments.indexOf(QStringLiteral("--settings"));
    if (i >= 0 && i + 1 < arguments.size())
        return arguments.at(i + 1);

    const QString portable = QCoreApplication::applicationDirPath() + QStringLiteral("/iwp.ini");
    if (QFileInfo::exists(portable))
        return portable;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/settings.ini");
}

} // namespace

void AppSettings::init(const QStringList &arguments)
{
    if (!g_instance)
        g_instance = new AppSettings(settingsPath(arguments));   // lives until the process ends
}

AppSettings &AppSettings::instance()
{
    if (!g_instance)
        init({});
    return *g_instance;
}

AppSettings::AppSettings(const QString &path) : m_s(path, QSettings::IniFormat)
{
    migrate();
}

// ----- versioning -----
// Every incompatible change of key names or meaning raises kVersion and adds a step here.
void AppSettings::migrate()
{
    const int version = m_s.value("meta/version", 0).toInt();
    if (version > kVersion) {
        qWarning() << "Settings file" << m_s.fileName() << "was written by a newer version (format"
                   << version << ") and may be partly ignored";
        return;
    }
    if (version < 1) {
        if (m_s.allKeys().isEmpty())
            importLegacy();   // first start of the INI era: take over what the old build saved
        // (a step for format 2 would go here: if (version < 2) { ... })
    }
    if (version != kVersion) {
        m_s.setValue("meta/version", kVersion);
        m_s.sync();
    }
}

// Early builds kept settings in the platform's native store (registry, plist).
void AppSettings::importLegacy()
{
    QSettings legacy(QSettings::NativeFormat, QSettings::UserScope,
                     QCoreApplication::organizationName(), QCoreApplication::applicationName());
    for (const QString &key : legacy.allKeys()) {
        // only our own keys: on some platforms the native store also returns system-wide ones
        if (key.startsWith(QLatin1String("mainwindow/")) || key.startsWith(QLatin1String("profiles/"))
            || key.startsWith(QLatin1String("ui/")))
            m_s.setValue(key, legacy.value(key));
    }
}

// ----- UI -----

QString AppSettings::language() const
{
    return m_s.value("ui/language", QStringLiteral("en")).toString();
}
void AppSettings::setLanguage(const QString &code)
{
    m_s.setValue("ui/language", code);
    m_s.sync();
}

QByteArray AppSettings::windowGeometry() const { return m_s.value("mainwindow/geometry").toByteArray(); }
void AppSettings::setWindowGeometry(const QByteArray &data) { m_s.setValue("mainwindow/geometry", data); }

QList<int> AppSettings::splitterSizes() const
{
    QList<int> sizes;
    for (const QVariant &v : m_s.value("mainwindow/splitter").toList())   // INI gives back strings
        sizes << v.toInt();
    return sizes;
}
void AppSettings::setSplitterSizes(const QList<int> &sizes)
{
    QVariantList list;
    for (int s : sizes)
        list << s;
    m_s.setValue("mainwindow/splitter", list);
}

QString AppSettings::lastDevice() const { return m_s.value("mainwindow/lastDevice").toString(); }
void AppSettings::setLastDevice(const QString &deviceId)
{
    m_s.setValue("mainwindow/lastDevice", deviceId);
    m_s.sync();
}

// ----- connection profiles -----
// profiles/<device>/<role>/choice
// profiles/<device>/<role>/configs/<choice>     (one JSON text per way of connecting)
// Older builds kept a single profiles/<device>/<role>/config for the chosen way: still read.

namespace {
QString profileBase(const QString &deviceId, const QString &role)
{
    return QStringLiteral("profiles/%1/%2/").arg(deviceId, role);
}
} // namespace

std::optional<QString> AppSettings::choice(const QString &deviceId, const QString &role) const
{
    const QString key = profileBase(deviceId, role) + QStringLiteral("choice");
    if (!m_s.contains(key))
        return std::nullopt;
    return m_s.value(key).toString();
}

void AppSettings::setChoice(const QString &deviceId, const QString &role, const QString &choice)
{
    m_s.setValue(profileBase(deviceId, role) + QStringLiteral("choice"), choice);
    m_s.sync();
}

QString AppSettings::config(const QString &deviceId, const QString &role, const QString &choice) const
{
    const QString base = profileBase(deviceId, role);
    const QString key = base + QStringLiteral("configs/") + choice;
    if (m_s.contains(key))
        return m_s.value(key).toString();
    if (m_s.value(base + QStringLiteral("choice")).toString() == choice)   // single config of older builds
        return m_s.value(base + QStringLiteral("config")).toString();
    return {};
}

void AppSettings::setConfig(const QString &deviceId, const QString &role, const QString &choice,
                            const QString &json)
{
    m_s.setValue(profileBase(deviceId, role) + QStringLiteral("configs/") + choice, json);
    m_s.sync();
}
