#include "iwp/keyvaluepage.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace iwp {

KeyValuePage::KeyValuePage(DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    m_status = new QLabel;
    m_table = new QTableWidget(0, 2);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->hide();
    m_table->setSortingEnabled(true);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_status);
    root->addWidget(m_table, 1);

    connect(m_model, &DeviceModel::valueChanged, this,
            [this](const QString &key, const QVariant &) { refresh(key); });
    connect(m_model, &DeviceModel::staleChanged, this,
            [this](const QString &key, bool) { refresh(key); });
    connect(m_model, &DeviceModel::statusChanged, this, &KeyValuePage::showStatus);

    for (const QString &key : m_model->keys())
        refresh(key);
    showStatus(m_model->status(), m_model->statusMessage());
}

void KeyValuePage::refresh(const QString &key)
{
    Row row = m_rows.value(key);
    if (!row.key) {
        m_table->setSortingEnabled(false);   // otherwise rows move while we fill them
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        row.key = new QTableWidgetItem(key);
        row.value = new QTableWidgetItem;
        m_table->setItem(r, 0, row.key);
        m_table->setItem(r, 1, row.value);
        m_table->setSortingEnabled(true);
        m_rows.insert(key, row);
    }

    const QVariant v = m_model->value(key);
    row.value->setText(v.typeId() == QMetaType::Double ? QString::number(v.toDouble(), 'g', 6)
                                                       : v.toString());
    const bool stale = m_model->isStale(key);   // grey = old value, the source is not answering
    const QBrush brush = stale ? palette().brush(QPalette::Disabled, QPalette::Text) : QBrush();
    row.key->setForeground(brush);
    row.value->setForeground(brush);
}

void KeyValuePage::showStatus(DeviceModel::Status status, const QString &message)
{
    static const char *names[] = {QT_TR_NOOP("Stopped"), QT_TR_NOOP("Connecting"),
                                  QT_TR_NOOP("Starting"), QT_TR_NOOP("Running"),
                                  QT_TR_NOOP("Offline"), QT_TR_NOOP("Error")};
    m_status->setText(tr(names[int(status)])
                      + (message.isEmpty() ? QString() : QStringLiteral(" - ") + message));
    m_status->setStyleSheet(status == DeviceModel::Status::Running
                                ? QStringLiteral("color: #2e7d32;")
                                : QStringLiteral("color: #c62828;"));
}

} // namespace iwp
