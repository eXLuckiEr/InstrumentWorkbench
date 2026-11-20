#include "iwp/configeditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace iwp {

ConfigEditor::ConfigEditor(QWidget *parent) : QWidget(parent) {}
ConfigEditor::~ConfigEditor() = default;

// ======================= JSON =======================

JsonConfigEditor::JsonConfigEditor(QWidget *parent) : ConfigEditor(parent)
{
    m_edit = new QLineEdit;
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_edit);
    connect(m_edit, &QLineEdit::textChanged, this, [this] { emit changed(); });
}

void JsonConfigEditor::setConfig(const QVariantMap &config)
{
    m_edit->setText(QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(config)).toJson(QJsonDocument::Compact)));
}

QVariantMap JsonConfigEditor::config() const
{
    const QJsonDocument doc = QJsonDocument::fromJson(m_edit->text().toUtf8());
    return doc.isObject() ? doc.object().toVariantMap() : QVariantMap();
}

QString JsonConfigEditor::validate() const
{
    if (m_edit->text().trimmed().isEmpty())
        return {};   // nothing = empty settings
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(m_edit->text().toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
        return tr("Invalid JSON: %1").arg(error.errorString());
    if (!doc.isObject())
        return tr("JSON object expected");
    return {};
}

// ======================= form =======================

FormConfigEditor::FormConfigEditor(QWidget *parent) : ConfigEditor(parent)
{
    m_form = new QFormLayout(this);
    m_form->setContentsMargins(0, 0, 0, 0);
}

FormConfigEditor::~FormConfigEditor() = default;

void FormConfigEditor::setConfig(const QVariantMap &config)
{
    QSignalBlocker blocker(this);   // filling the form is not a change made by the user
    m_rest = config;
    for (const Binding &b : std::as_const(m_bindings)) {
        m_rest.remove(b.key);
        if (config.contains(b.key))
            b.set(config.value(b.key));   // missing key: the field keeps its default
    }
}

QVariantMap FormConfigEditor::config() const
{
    QVariantMap out = m_rest;
    for (const Binding &b : m_bindings) {
        const QVariant v = b.get();
        if (v.isValid())
            out.insert(b.key, v);
        else
            out.remove(b.key);
    }
    return out;
}

void FormConfigEditor::bind(const QString &key, std::function<QVariant()> get,
                            std::function<void(const QVariant &)> set)
{
    m_bindings.append({key, std::move(get), std::move(set)});
}

QLineEdit *FormConfigEditor::addText(const QString &key, const QString &label,
                                     const QString &placeholder)
{
    auto *edit = new QLineEdit;
    edit->setPlaceholderText(placeholder);
    m_form->addRow(label, edit);
    bind(key,
         [edit]() -> QVariant {
             const QString text = edit->text().trimmed();
             return text.isEmpty() ? QVariant() : QVariant(text);
         },
         [edit](const QVariant &v) { edit->setText(v.toString()); });
    connect(edit, &QLineEdit::textChanged, this, [this] { emit changed(); });
    return edit;
}

QLineEdit *FormConfigEditor::addFile(const QString &key, const QString &label,
                                     const QString &placeholder)
{
    auto *edit = new QLineEdit;
    edit->setPlaceholderText(placeholder);
    auto *browse = new QPushButton(QStringLiteral("..."));
    browse->setFixedWidth(32);
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    m_form->addRow(label, row);

    connect(browse, &QPushButton::clicked, this, [this, edit, label] {
        const QString file = QFileDialog::getOpenFileName(this, label, edit->text());
        if (!file.isEmpty())
            edit->setText(file);
    });
    bind(key,
         [edit]() -> QVariant {
             const QString text = edit->text().trimmed();
             return text.isEmpty() ? QVariant() : QVariant(text);
         },
         [edit](const QVariant &v) { edit->setText(v.toString()); });
    connect(edit, &QLineEdit::textChanged, this, [this] { emit changed(); });
    return edit;
}

QSpinBox *FormConfigEditor::addInt(const QString &key, const QString &label, int minimum,
                                   int maximum, const QString &suffix)
{
    auto *box = new QSpinBox;
    box->setRange(minimum, maximum);
    box->setSuffix(suffix);
    m_form->addRow(label, box);
    bind(key, [box]() -> QVariant { return box->value(); },
         [box](const QVariant &v) { box->setValue(v.toInt()); });
    connect(box, &QSpinBox::valueChanged, this, [this] { emit changed(); });
    return box;
}

QComboBox *FormConfigEditor::addChoice(const QString &key, const QString &label,
                                       const ChoiceItems &items, bool editable)
{
    auto *combo = new QComboBox;
    combo->setEditable(editable);
    for (const auto &item : items)
        combo->addItem(item.first, item.second);
    m_form->addRow(label, combo);
    bindCombo(key, combo);
    return combo;
}

QCheckBox *FormConfigEditor::addFlag(const QString &key, const QString &label)
{
    auto *box = new QCheckBox;
    m_form->addRow(label, box);
    bind(key, [box]() -> QVariant { return box->isChecked(); },
         [box](const QVariant &v) { box->setChecked(v.toBool()); });
    connect(box, &QCheckBox::toggled, this, [this] { emit changed(); });
    return box;
}

void FormConfigEditor::bindCombo(const QString &key, QComboBox *combo)
{
    bind(key,
         [combo]() -> QVariant {
             const int i = combo->currentIndex();
             if (!combo->isEditable())
                 return i >= 0 ? combo->itemData(i) : QVariant();
             const QString text = combo->currentText().trimmed();
             if (i >= 0 && text == combo->itemText(i))
                 return combo->itemData(i);      // a listed entry: its value, not its label
             if (text.isEmpty())
                 return QVariant();
             bool ok = false;                    // typed number in a list of numbers stays a number
             const int number = text.toInt(&ok);
             if (ok && combo->count() > 0 && combo->itemData(0).typeId() == QMetaType::Int)
                 return number;
             return text;
         },
         [combo](const QVariant &v) {
             int i = combo->findData(v);
             if (i < 0)
                 i = combo->findText(v.toString());   // e.g. 115200 that came back from JSON as a double
             if (i >= 0)
                 combo->setCurrentIndex(i);
             else if (combo->isEditable())
                 combo->setEditText(v.toString());
             else {                                    // keep a value this list does not know
                 combo->addItem(v.toString(), v);
                 combo->setCurrentIndex(combo->count() - 1);
             }
         });
    if (combo->isEditable())
        connect(combo, &QComboBox::editTextChanged, this, [this] { emit changed(); });
    connect(combo, &QComboBox::currentIndexChanged, this, [this] { emit changed(); });
}

} // namespace iwp
