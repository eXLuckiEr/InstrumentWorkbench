#pragma once
#include <QHash>
#include <QString>

#include "historycore.h"

// Command history of the console's Line view, kept per connection (e.g. "192.168.0.50:5025", "COM3")
// in <AppConfigLocation>/console_history.json together with the view's preferences:
//   { "last": "<connection>", "prefs": {"ending": 1, "delay": 50, "limit": 100},
//     "connections": { "<connection>": [ {"t": "*IDN?\nSYST:ERR?", "p": true}, ... ] } }
// (entries newest first). Plain text: do not put secrets into commands.
class CommandHistory
{
public:
    CommandHistory();
    explicit CommandHistory(const QString &path);

    static QString defaultPath();

    // Switches to the history of a connection (loads it). Empty = the one used last.
    void setConnection(const QString &connection);
    QString connection() const { return m_connection; }

    cmdhist::HistoryCore &core() { return m_core; }
    const cmdhist::HistoryCore &core() const { return m_core; }
    void save() const;

    int pref(const QString &name, int fallback) const;
    void setPref(const QString &name, int value);

private:
    void load();

    QString m_path;
    QString m_connection;
    QString m_last;
    cmdhist::HistoryCore m_core;
    QHash<QString, int> m_prefs;
};
