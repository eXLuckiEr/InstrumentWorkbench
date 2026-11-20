#include "telemetrypage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "iwp/plotview.h"

TelemetryPage::TelemetryPage(iwp::DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    m_status = new QLabel;

    auto *data = new QGroupBox(tr("Data"));
    auto *dg = new QGridLayout(data);
    iwp::PlotBuffer *plotData = m_model->plot(QStringLiteral("signals"));
    for (int i = 1; i <= 6; ++i) {
        dg->addWidget(new QLabel(QStringLiteral("DATA%1").arg(i)), i - 1, 0);
        dg->addWidget(addValue(QStringLiteral("d%1").arg(i)), i - 1, 1);
        if (plotData) {   // the same view as the big plot, in a small area: the last 30 s
            auto *spark = new iwp::PlotView(plotData);
            spark->setStyle(iwp::PlotView::Style::Compact);
            spark->setSeries({i - 1});
            spark->setWindowSeconds(30);
            spark->setFixedSize(150, 26);
            dg->addWidget(spark, i - 1, 2);
        }
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
    auto *right = new QVBoxLayout;
    if (plotData)
        right->addWidget(buildPlotPanel(), 1);   // the centre of attention
    right->addWidget(mux);
    cols->addLayout(right, 1);
    root->addLayout(cols, 1);

    connect(m_model, &iwp::DeviceModel::valueChanged, this,
            [this](const QString &key, const QVariant &) { refresh(key); });
    connect(m_model, &iwp::DeviceModel::staleChanged, this,
            [this](const QString &key, bool) { refresh(key); });
    connect(m_model, &iwp::DeviceModel::statusChanged, this, &TelemetryPage::showStatus);

    for (const QString &key : m_model->keys())
        refresh(key);
    showStatus(m_model->status(), m_model->statusMessage());
}

QWidget *TelemetryPage::buildPlotPanel()
{
    auto *panel = new QWidget;
    auto *lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 0, 0, 0);

    m_plot = new iwp::PlotView(m_model->plot(QStringLiteral("signals")));
    m_plot->setMinimumHeight(220);

    auto *bar = new QHBoxLayout;
    bar->addWidget(new QLabel(tr("X axis:")));
    m_xaxis = new QComboBox;
    m_xaxis->addItem(tr("Time"));
    m_xaxis->addItem(tr("Samples"));
    bar->addWidget(m_xaxis);
    bar->addWidget(new QLabel(tr("Window:")));
    m_window = new QComboBox;
    bar->addWidget(m_window);

    auto *autoY = new QCheckBox(tr("Auto Y"));
    autoY->setChecked(true);
    auto *pause = new QCheckBox(tr("Pause"));
    bar->addWidget(autoY);
    bar->addWidget(pause);
    bar->addSpacing(12);
    for (int i = 0; i < 6; ++i) {   // which curves are drawn
        m_series[i] = new QCheckBox(QStringLiteral("%1").arg(i + 1));
        m_series[i]->setChecked(true);
        m_series[i]->setToolTip(tr("Show DATA%1").arg(i + 1));
        bar->addWidget(m_series[i]);
        connect(m_series[i], &QCheckBox::toggled, this, &TelemetryPage::applySeries);
    }
    bar->addStretch(1);

    fillWindowChoices();
    connect(m_xaxis, &QComboBox::currentIndexChanged, this, [this] { fillWindowChoices(); });
    connect(m_window, &QComboBox::currentIndexChanged, this, [this] { applyWindow(); });
    connect(autoY, &QCheckBox::toggled, m_plot, [this](bool on) {
        if (on)
            m_plot->setAutoRange(true);
        else
            m_plot->setRange(-10, 10);
    });
    connect(pause, &QCheckBox::toggled, m_plot, &iwp::PlotView::setPaused);

    lay->addLayout(bar);
    lay->addWidget(m_plot, 1);
    return panel;
}

void TelemetryPage::fillWindowChoices()
{
    const bool time = m_xaxis->currentIndex() == 0;
    m_plot->setXAxis(time ? iwp::PlotView::XAxis::Time : iwp::PlotView::XAxis::Samples);
    const QSignalBlocker block(m_window);
    m_window->clear();
    if (time) {
        for (double s : {1.0, 5.0, 10.0, 30.0, 60.0, 300.0})
            m_window->addItem(s >= 60 ? tr("%1 min").arg(s / 60) : tr("%1 s").arg(s), s);
        m_window->setCurrentIndex(2);
    } else {
        for (qint64 n : {500, 2000, 10000, 100000, 1000000})
            m_window->addItem(tr("%1 samples").arg(n), n);
        m_window->setCurrentIndex(2);
    }
    applyWindow();
}

void TelemetryPage::applyWindow()
{
    if (m_xaxis->currentIndex() == 0)
        m_plot->setWindowSeconds(m_window->currentData().toDouble());
    else
        m_plot->setWindowSamples(m_window->currentData().toLongLong());
}

void TelemetryPage::applySeries()
{
    QList<int> shown;
    for (int i = 0; i < 6; ++i)
        if (m_series[i]->isChecked())
            shown.append(i);
    m_plot->setSeries(shown.isEmpty() ? QList<int>{-1} : shown);   // {-1}: draw nothing
}

QLabel *TelemetryPage::addValue(const QString &key)
{
    auto *label = new QLabel(QStringLiteral("-"));
    label->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // Fixed size: a QWidget with min == max size does not invalidate the parent layout on setText(),
    // so a value update repaints only the label instead of re-running the page layout.
    label->setFixedSize(QFontMetrics(label->font()).horizontalAdvance(QStringLiteral("-88888.8888")) + 8,
                        QFontMetrics(label->font()).height() + 4);
    m_labels.insert(key, label);
    return label;
}

void TelemetryPage::refresh(const QString &key)
{
    QLabel *label = m_labels.value(key, nullptr);
    if (!label)
        return;
    const QVariant v = m_model->value(key);
    const QString text = v.typeId() == QMetaType::Double ? QString::number(v.toDouble(), 'f', 4)
                                                         : v.toString();
    if (label->text() != text)
        label->setText(text);
    const bool enabled = !m_model->isStale(key);   // greyed out when the stream is silent
    if (label->isEnabled() != enabled)
        label->setEnabled(enabled);
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
