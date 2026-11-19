#include "telemetrypage.h"

#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

TelemetryPage::TelemetryPage(iwp::DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    m_status = new QLabel;

    auto *data = new QGroupBox(tr("Data"));
    auto *dg = new QGridLayout(data);
    for (int i = 1; i <= 6; ++i) {
        dg->addWidget(new QLabel(QStringLiteral("DATA%1").arg(i)), i - 1, 0);
        dg->addWidget(addValue(QStringLiteral("d%1").arg(i)), i - 1, 1);
    }
    dg->addWidget(new QLabel(tr("Counter / ID")), 6, 0);
    auto *ids = new QHBoxLayout;
    ids->addWidget(addValue(QStringLiteral("counter")));
    ids->addWidget(addValue(QStringLiteral("id")));
    dg->addLayout(ids, 6, 1);
    dg->setColumnStretch(1, 1);

    auto *mux = new QGroupBox(tr("MUX DATA (raw, counter mod 16)"));
    auto *mg = new QGridLayout(mux);
    for (int i = 0; i < 16; ++i) {
        mg->addWidget(new QLabel(QStringLiteral("[%1]").arg(i)), i / 4, (i % 4) * 2);
        mg->addWidget(addValue(QStringLiteral("mux%1").arg(i)), i / 4, (i % 4) * 2 + 1);
    }

    auto *stats = new QGroupBox(tr("Link quality"));
    auto *sg = new QGridLayout(stats);
    const struct { const char *key; const char *title; } rows[] = {
        {"fps", QT_TR_NOOP("Frames/s")}, {"frames_ok", QT_TR_NOOP("Frames OK")},
        {"frames_lost", QT_TR_NOOP("Lost (counter gaps)")},
        {"crc_errors", QT_TR_NOOP("CRC errors")}, {"dropped_bytes", QT_TR_NOOP("Dropped bytes")}};
    int r = 0;
    for (const auto &row : rows) {
        sg->addWidget(new QLabel(tr(row.title)), r, 0);
        sg->addWidget(addValue(QString::fromLatin1(row.key)), r, 1);
        ++r;
    }
    sg->setColumnStretch(1, 1);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_status);
    auto *cols = new QHBoxLayout;
    auto *left = new QVBoxLayout;
    left->addWidget(data);
    left->addWidget(stats);
    cols->addLayout(left);
    cols->addWidget(mux, 1);
    root->addLayout(cols);
    root->addStretch(1);

    connect(m_model, &iwp::DeviceModel::valueChanged, this,
            [this](const QString &key, const QVariant &) { refresh(key); });
    connect(m_model, &iwp::DeviceModel::staleChanged, this,
            [this](const QString &key, bool) { refresh(key); });
    connect(m_model, &iwp::DeviceModel::statusChanged, this, &TelemetryPage::showStatus);

    for (const QString &key : m_model->keys())
        refresh(key);
    showStatus(m_model->status(), m_model->statusMessage());
}

QLabel *TelemetryPage::addValue(const QString &key)
{
    auto *label = new QLabel(QStringLiteral("-"));
    label->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    label->setMinimumWidth(80);
    m_labels.insert(key, label);
    return label;
}

void TelemetryPage::refresh(const QString &key)
{
    QLabel *label = m_labels.value(key, nullptr);
    if (!label)
        return;
    const QVariant v = m_model->value(key);
    label->setText(v.typeId() == QMetaType::Double ? QString::number(v.toDouble(), 'f', 4)
                                                   : v.toString());
    label->setEnabled(!m_model->isStale(key));   // greyed out when the stream is silent
}

void TelemetryPage::showStatus(iwp::DeviceModel::Status status, const QString &message)
{
    static const char *names[] = {"Stopped", "Connecting", "Starting",
                                  "Running", "Offline", "Error"};
    m_status->setText(QLatin1String(names[int(status)])
                      + (message.isEmpty() ? QString() : QStringLiteral(" - ") + message));
    m_status->setStyleSheet(status == iwp::DeviceModel::Status::Running
                                ? QStringLiteral("color: #2e7d32;")
                                : QStringLiteral("color: #c62828;"));
}
