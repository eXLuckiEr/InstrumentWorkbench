#include "rolerow.h"

#include <QComboBox>
#include <QGridLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QPushButton>

#include "appsettings.h"
#include "iwp/configeditor.h"
#include "pluginmanager.h"

namespace {

constexpr int kDefaultsRole = Qt::UserRole + 1;

QString toJson(const QVariantMap &map)
{
    return QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(map)).toJson(QJsonDocument::Compact));
}

QVariantMap fromJson(const QString &text)
{
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    return doc.isObject() ? doc.object().toVariantMap() : QVariantMap();
}

} // namespace

RoleRow::RoleRow(const iwp::EndpointRole &role, const QString &deviceId, PluginManager &plugins,
                 QGridLayout *layout, int gridRow, QObject *parent)
    : QObject(parent), m_role(role), m_deviceId(deviceId), m_plugins(plugins)
{
    m_combo = new QComboBox;
    if (role.optional)
        m_combo->addItem(tr("- none -"), QString());
    for (const auto &choice : m_plugins.choicesFor(role.kind)) {
        QVariantMap defaults = choice.defaults;
        for (auto it = role.defaults.cbegin(); it != role.defaults.cend(); ++it)
            defaults.insert(it.key(), it.value());   // the device knows better
        m_combo->addItem(choice.label, choice.id);
        m_combo->setItemData(m_combo->count() - 1, defaults, kDefaultsRole);
    }

    m_stack = new QStackedWidget;
    m_jsonButton = new QPushButton;   // the same kind of widget as the Connect button: same metrics
    m_jsonButton->setText(QStringLiteral("{ }"));
    m_jsonButton->setCheckable(true);
    m_jsonButton->setToolTip(tr("Edit as JSON"));
    m_jsonButton->setFixedWidth(m_jsonButton->fontMetrics().horizontalAdvance(m_jsonButton->text()) + 28);   // compact, not the default push button width

    auto *label = new QLabel(QStringLiteral("%1 (%2)").arg(role.role, role.kind));
    label->setToolTip(role.description);

    // Two grid rows per role. Line 1 holds widgets of one kind only (label, combo box, "{ }"), so Qt
    // centres them against each other, the same on every platform. Line 2 is the settings form of
    // the chosen way, over the full width: whatever its structure, it never has to line up with
    // anything.   | label | combo | ........ stretch ........ | { } | (Connect: see MainWindow) |
    layout->addWidget(label, gridRow, 0);
    layout->addWidget(m_combo, gridRow, 1);
    layout->addWidget(m_jsonButton, gridRow, kJsonColumn);
    layout->addWidget(m_stack, gridRow + 1, 0, 1, kColumns);

    // the way chosen last time (if its plugin is still there)
    if (const auto saved = AppSettings::instance().choice(deviceId, role.role)) {
        const int idx = m_combo->findData(*saved);
        if (idx >= 0)
            m_combo->setCurrentIndex(idx);
    }
    m_current = choiceId();
    showChoice(m_current);

    connect(m_combo, &QComboBox::currentIndexChanged, this, &RoleRow::onChoiceChanged);
    connect(m_jsonButton, &QPushButton::toggled, this, &RoleRow::onJsonToggled);
}

QString RoleRow::choiceId() const { return m_combo->currentData().toString(); }

iwp::ConfigEditor *RoleRow::active() const
{
    return (m_json && m_jsonButton->isChecked()) ? static_cast<iwp::ConfigEditor *>(m_json) : m_editor;
}

QVariantMap RoleRow::config() const { return active() ? active()->config() : QVariantMap(); }

QString RoleRow::validate() const { return active() ? active()->validate() : QString(); }

QVariantMap RoleRow::defaultsFor(const QString &choiceId) const
{
    const int idx = m_combo->findData(choiceId);
    return idx >= 0 ? m_combo->itemData(idx, kDefaultsRole).toMap() : QVariantMap();
}

// Defaults of the plugin, with what the user saved on top (so that keys added by a newer
// plugin version appear, and saved values win).
QVariantMap RoleRow::configFor(const QString &choiceId) const
{
    const auto cached = m_configs.constFind(choiceId);
    if (cached != m_configs.constEnd())
        return *cached;
    QVariantMap cfg = defaultsFor(choiceId);
    const QVariantMap saved = fromJson(AppSettings::instance().config(m_deviceId, m_role.role, choiceId));
    for (auto it = saved.cbegin(); it != saved.cend(); ++it)
        cfg.insert(it.key(), it.value());
    return cfg;
}

void RoleRow::showChoice(const QString &choiceId)
{
    while (m_stack->count() > 0) {
        QWidget *w = m_stack->widget(0);
        m_stack->removeWidget(w);
        delete w;
    }
    m_editor = nullptr;
    m_json = nullptr;
    {
        QSignalBlocker blocker(m_jsonButton);
        m_jsonButton->setChecked(false);
    }
    if (choiceId.isEmpty()) {
        m_jsonButton->hide();
        return;
    }

    iwp::ConfigEditor *custom = m_plugins.createConfigEditor(choiceId, m_role.kind, m_stack);
    if (custom) {
        m_editor = custom;
        m_json = new iwp::JsonConfigEditor(m_stack);   // for settings the form does not show
    } else {
        m_editor = new iwp::JsonConfigEditor(m_stack);
    }
    m_stack->addWidget(m_editor);
    if (m_json)
        m_stack->addWidget(m_json);
    m_jsonButton->setVisible(custom != nullptr);
    m_editor->setConfig(configFor(choiceId));
}

void RoleRow::onChoiceChanged()
{
    // keep what was typed for the way we are leaving
    if (m_editor && !m_current.isEmpty()) {
        const bool jsonMode = m_json && m_jsonButton->isChecked();
        if (!jsonMode || m_json->validate().isEmpty())
            m_configs.insert(m_current, active()->config());
    }
    m_current = choiceId();
    showChoice(m_current);
}

void RoleRow::onJsonToggled(bool on)
{
    if (!m_json || !m_editor)
        return;
    if (on) {
        m_json->setConfig(m_editor->config());
        m_stack->setCurrentWidget(m_json);
        return;
    }
    const QString error = m_json->validate();
    if (!error.isEmpty()) {                      // cannot go back to the form with broken JSON
        QSignalBlocker blocker(m_jsonButton);
        m_jsonButton->setChecked(true);
        emit problem(tr("Role '%1': %2").arg(m_role.role, error));
        return;
    }
    m_editor->setConfig(m_json->config());
    m_stack->setCurrentWidget(m_editor);
}

void RoleRow::setLocked(bool locked)
{
    m_combo->setEnabled(!locked);
    m_stack->setEnabled(!locked);
    m_jsonButton->setEnabled(!locked);
}

void RoleRow::save() const
{
    AppSettings &settings = AppSettings::instance();
    settings.setChoice(m_deviceId, m_role.role, m_current);
    for (auto it = m_configs.cbegin(); it != m_configs.cend(); ++it)
        if (it.key() != m_current)
            settings.setConfig(m_deviceId, m_role.role, it.key(), toJson(it.value()));
    if (!m_current.isEmpty() && active())
        settings.setConfig(m_deviceId, m_role.role, m_current, toJson(active()->config()));
}
