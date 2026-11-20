#pragma once
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include "iwp/ifactories.h"

class QComboBox;
class QGridLayout;
class QStackedWidget;
class QToolButton;
class PluginManager;
namespace iwp {
class ConfigEditor;
class JsonConfigEditor;
}

// One line of the connection panel: endpoint role, the way to connect (combo box) and the
// settings of that way (a form from the plugin, or JSON). Remembers the settings of every way
// that was chosen, and keeps the profile in AppSettings.
class RoleRow : public QObject
{
    Q_OBJECT
public:
    // Puts its widgets into row `gridRow` of `layout` (columns 0..3).
    RoleRow(const iwp::EndpointRole &role, const QString &deviceId, PluginManager &plugins,
            QGridLayout *layout, int gridRow, QObject *parent = nullptr);

    const iwp::EndpointRole &role() const { return m_role; }
    QString choiceId() const;          // "transport/serial"; empty = nothing chosen
    QVariantMap config() const;        // settings of the chosen way
    QString validate() const;          // empty = fine, otherwise a message
    void save() const;                 // profile -> AppSettings

signals:
    void problem(const QString &message);

private:
    iwp::ConfigEditor *active() const;
    QVariantMap defaultsFor(const QString &choiceId) const;
    QVariantMap configFor(const QString &choiceId) const;
    void showChoice(const QString &choiceId);
    void onChoiceChanged();
    void onJsonToggled(bool on);

    iwp::EndpointRole m_role;
    QString m_deviceId;
    PluginManager &m_plugins;

    QComboBox *m_combo = nullptr;
    QStackedWidget *m_stack = nullptr;
    QToolButton *m_jsonButton = nullptr;
    iwp::ConfigEditor *m_editor = nullptr;       // the plugin's form, or JSON when there is none
    iwp::JsonConfigEditor *m_json = nullptr;     // JSON view next to a plugin's form
    QString m_current;                           // choice shown now
    QHash<QString, QVariantMap> m_configs;       // settings of the other ways, kept while the app runs
};
