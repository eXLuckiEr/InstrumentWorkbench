#pragma once
#include <QList>
#include <QVariantMap>
#include <QWidget>
#include <functional>
#include <utility>

#include "iwp/global.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QSpinBox;

namespace iwp {

// Editor of the connection settings of one transport/endpoint (the JSON that the
// plugin receives in open()). The host shows the editor a plugin supplies
// (ITransportFactory/IEndpointFactory::createConfigEditor), or JsonConfigEditor.
// UI thread only; the session and channels only ever get the finished QVariantMap.
class IWP_SDK_EXPORT ConfigEditor : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigEditor(QWidget *parent = nullptr);
    ~ConfigEditor() override;

    // Show this config. Keys the editor does not know must survive: config() returns them again.
    virtual void setConfig(const QVariantMap &config) = 0;
    virtual QVariantMap config() const = 0;
    // Empty string = settings are fine, otherwise a message for the user.
    virtual QString validate() const { return {}; }

signals:
    void changed();
};

// The settings as one line of JSON: works for every plugin, no knowledge of keys needed.
class IWP_SDK_EXPORT JsonConfigEditor : public ConfigEditor
{
    Q_OBJECT
public:
    explicit JsonConfigEditor(QWidget *parent = nullptr);
    void setConfig(const QVariantMap &config) override;
    QVariantMap config() const override;
    QString validate() const override;

private:
    QLineEdit *m_edit;
};

using ChoiceItems = QList<std::pair<QString, QVariant>>;   // {text shown, value stored}

// Base for hand-made forms. Add fields in the constructor with the add...() helpers: each
// creates the widget, puts it into the form and binds it to a config key. Keys of the config
// that no field is bound to are kept untouched (device-specific keys such as "terminator"
// or rarely used options), so the form never loses data it does not show.
//
// A text field that is left empty removes its key from the config (use defaults of the plugin).
class IWP_SDK_EXPORT FormConfigEditor : public ConfigEditor
{
    Q_OBJECT
public:
    explicit FormConfigEditor(QWidget *parent = nullptr);
    ~FormConfigEditor() override;

    void setConfig(const QVariantMap &config) override;
    QVariantMap config() const override;

protected:
    QFormLayout *form() const { return m_form; }

    QLineEdit *addText(const QString &key, const QString &label, const QString &placeholder = {});
    QLineEdit *addFile(const QString &key, const QString &label, const QString &placeholder = {});
    QSpinBox *addInt(const QString &key, const QString &label, int minimum, int maximum,
                     const QString &suffix = {});
    // editable: the user may type a value that is not in the list (numbers stay numbers)
    QComboBox *addChoice(const QString &key, const QString &label, const ChoiceItems &items,
                         bool editable = false);
    QCheckBox *addFlag(const QString &key, const QString &label);

    // For fields built by hand. get() returns an invalid QVariant to leave the key out.
    void bind(const QString &key, std::function<QVariant()> get,
              std::function<void(const QVariant &)> set);
    void bindCombo(const QString &key, QComboBox *combo);

private:
    struct Binding {
        QString key;
        std::function<QVariant()> get;
        std::function<void(const QVariant &)> set;
    };
    QFormLayout *m_form;
    QList<Binding> m_bindings;
    QVariantMap m_rest;
};

} // namespace iwp
