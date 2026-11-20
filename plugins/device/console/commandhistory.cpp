#include "commandhistory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr int kDefaultLimit = 100;

QJsonObject readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

} // namespace

QString CommandHistory::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + QStringLiteral("/console_history.json");
}

CommandHistory::CommandHistory() : CommandHistory(defaultPath()) {}

CommandHistory::CommandHistory(const QString &path) : m_path(path)
{
    load();
}

void CommandHistory::load()
{
    const QJsonObject root = readFile(m_path);
    m_last = root.value(QStringLiteral("last")).toString();
    const QJsonObject prefs = root.value(QStringLiteral("prefs")).toObject();
    m_prefs.clear();
    for (auto it = prefs.begin(); it != prefs.end(); ++it)
        m_prefs.insert(it.key(), it.value().toInt());
    const int limit = qBound(10, m_prefs.value(QStringLiteral("limit"), kDefaultLimit), 10000);
    m_core = cmdhist::HistoryCore(std::size_t(limit));
}

void CommandHistory::setConnection(const QString &connection)
{
    m_connection = connection.isEmpty() ? (m_last.isEmpty() ? QStringLiteral("default") : m_last)
                                        : connection;
    const QJsonArray list = readFile(m_path).value(QStringLiteral("connections")).toObject()
                                .value(m_connection).toArray();
    std::vector<cmdhist::Entry> entries;
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        entries.push_back({o.value(QStringLiteral("t")).toString().toStdString(),
                           o.value(QStringLiteral("p")).toBool()});
    }
    m_core.assign(entries);
}

void CommandHistory::save() const
{
    // Read-modify-write: other connections (and a second window) keep what they have.
    QJsonObject root = readFile(m_path);

    QJsonArray list;
    for (const cmdhist::Entry &e : m_core.raw()) {
        QJsonObject o;
        o.insert(QStringLiteral("t"), QString::fromStdString(e.text));
        if (e.pinned)
            o.insert(QStringLiteral("p"), true);
        list.append(o);
    }
    QJsonObject connections = root.value(QStringLiteral("connections")).toObject();
    connections.insert(m_connection, list);
    root.insert(QStringLiteral("connections"), connections);
    root.insert(QStringLiteral("last"), m_connection);

    QJsonObject prefs = root.value(QStringLiteral("prefs")).toObject();
    for (auto it = m_prefs.cbegin(); it != m_prefs.cend(); ++it)
        prefs.insert(it.key(), it.value());
    root.insert(QStringLiteral("prefs"), prefs);

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

int CommandHistory::pref(const QString &name, int fallback) const
{
    return m_prefs.value(name, fallback);
}

void CommandHistory::setPref(const QString &name, int value)
{
    m_prefs.insert(name, value);
}
