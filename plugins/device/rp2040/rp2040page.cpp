#include "rp2040page.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <QShowEvent>
#include <QTabWidget>

#include "iwp/plotview.h"
#include "picoboardview.h"

namespace {
QString dash() { return QStringLiteral("—"); }

QString uptimeText(qlonglong s)
{
    const qlonglong d = s / 86400, h = (s / 3600) % 24, m = (s / 60) % 60, sec = s % 60;
    const QString hms = QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(sec, 2, 10, QLatin1Char('0'));
    return d > 0 ? QStringLiteral("%1 d %2").arg(d).arg(hms) : hms;
}
} // namespace

Rp2040Page::Rp2040Page(iwp::DeviceModel *model, QWidget *parent) : QWidget(parent), m_model(model)
{
    build();
    for (const QString &key : m_model->keys())
        if (!key.startsWith(QLatin1String("gpio_")))
            showValue(key);
    connect(m_model, &iwp::DeviceModel::valueChanged, this, [this](const QString &key) {
        if (key == QLatin1String("gpio_map") || key == QLatin1String("gpio_error"))
            updateGpio();
        else
            showValue(key);
    });
    connect(m_model, &iwp::DeviceModel::staleChanged, this, [this](const QString &key, bool stale) {
        if (QLabel *l = m_labels.value(key))
            l->setEnabled(!stale);
        if (key == QLatin1String("temp_c"))
            m_temp->setEnabled(!stale);
        if (key == QLatin1String("gpio_map"))
            m_board->setStale(stale);
    });
    connect(m_model, &iwp::DeviceModel::statusChanged, this, [this](iwp::DeviceModel::Status s, const QString &) {
        showStatus();
        if (s == iwp::DeviceModel::Status::Running)
            updateGpioPolling();
    });
    updateGpio();
    showStatus();
}

QLabel *Rp2040Page::addRow(QFormLayout *form, const QString &title, const QString &key)
{
    auto *l = new QLabel(dash());
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(title, l);
    m_labels.insert(key, l);
    return l;
}

void Rp2040Page::build()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_tabs = new QTabWidget;
    m_tabs->addTab(buildOverview(), tr("Overview"));
    m_tabs->addTab(buildGpio(), tr("GPIO"));
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] { updateGpioPolling(); });
    root->addWidget(m_tabs);
}

QWidget *Rp2040Page::buildOverview()
{
    auto *page = new QWidget;
    auto *root = new QVBoxLayout(page);

    auto *top = new QHBoxLayout;
    // what is flashed into the board
    auto *info = new QGroupBox(tr("Firmware"));
    auto *form = new QFormLayout(info);
    addRow(form, tr("Manufacturer:"), QStringLiteral("manufacturer"));
    addRow(form, tr("Model:"), QStringLiteral("model"));
    addRow(form, tr("Firmware version:"), QStringLiteral("firmware"));
    addRow(form, tr("Chip ID:"), QStringLiteral("uid"));
    addRow(form, tr("SCPI version:"), QStringLiteral("scpi_version"));
    addRow(form, tr("CPU clock:"), QStringLiteral("clock_hz"));
    addRow(form, tr("Uptime:"), QStringLiteral("uptime_s"));
    addRow(form, tr("Free heap (lowest):"), QStringLiteral("heap_free"));
    top->addWidget(info, 3);

    // temperature
    auto *tbox = new QGroupBox(tr("Board temperature"));
    auto *tl = new QVBoxLayout(tbox);
    m_temp = new QLabel(dash());
    QFont f = m_temp->font();
    f.setPointSizeF(f.pointSizeF() * 3);
    f.setBold(true);
    m_temp->setFont(f);
    m_temp->setAlignment(Qt::AlignCenter);
    m_range = new QLabel;
    m_range->setAlignment(Qt::AlignCenter);
    m_status = new QLabel;
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setWordWrap(true);
    tl->addStretch(1);
    tl->addWidget(m_temp);
    tl->addWidget(m_range);
    tl->addStretch(1);
    tl->addWidget(m_status);
    top->addWidget(tbox, 2);
    root->addLayout(top);

    // history
    if (iwp::PlotBuffer *buffer = m_model->plot(QStringLiteral("temperature"))) {
        auto *plot = new iwp::PlotView(buffer);
        plot->setWindowSeconds(300);
        plot->setMinimumHeight(140);
        root->addWidget(plot, 1);
    }
    return page;
}

QWidget *Rp2040Page::buildGpio()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    m_gpioNote = new QLabel;
    m_gpioNote->setWordWrap(true);
    m_gpioNote->setStyleSheet(QStringLiteral("color: #c62828;"));
    v->addWidget(m_gpioNote);
    m_board = new PicoBoardView;
    v->addWidget(m_board, 1);
    auto *hint = new QLabel(tr("Raspberry Pi Pico, USB at the top. Filled dot = high level, ↑ ↓ = pull-up / pull-down. "
                               "The text shows what the pin is connected to; hover a pin for details. "
                               "The state is read twice a second while this tab is open."));
    hint->setWordWrap(true);
    v->addWidget(hint);
    return page;
}

void Rp2040Page::updateGpio()
{
    const QVariantList list = m_model->value(QStringLiteral("gpio_map")).toList();
    QVector<PicoBoardView::Pin> pins(30);
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        const int n = m.value("pin").toInt();
        if (n < 0 || n >= pins.size())
            continue;
        PicoBoardView::Pin &p = pins[n];
        p.valid = true;
        p.function = m.value("function").toString();
        p.output = m.value("output").toBool();
        p.level = m.value("level").toBool();
        p.pull = m.value("pull").toString();
    }
    m_board->setPins(pins);
    m_board->setStale(m_model->isStale(QStringLiteral("gpio_map")));
    m_gpioNote->setText(m_model->value(QStringLiteral("gpio_error")).toString());
}

// The GPIO map is a long answer: the session reads it only while it is visible.
void Rp2040Page::updateGpioPolling()
{
    const bool want = isVisible() && m_tabs->currentIndex() == 1;
    m_model->sendCommand(QStringLiteral("gpioPolling"), want);
}

void Rp2040Page::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    updateGpioPolling();
}

void Rp2040Page::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    m_model->sendCommand(QStringLiteral("gpioPolling"), false);
}

void Rp2040Page::showValue(const QString &key)
{
    const QVariant v = m_model->value(key);
    if (key == QLatin1String("temp_c")) {
        const double t = v.toDouble();
        m_temp->setText(QStringLiteral("%1 °C").arg(t, 0, 'f', 1));
        if (!m_haveRange) {
            m_min = m_max = t;
            m_haveRange = true;
        }
        m_min = qMin(m_min, t);
        m_max = qMax(m_max, t);
        m_range->setText(tr("min %1 °C   max %2 °C").arg(m_min, 0, 'f', 1).arg(m_max, 0, 'f', 1));
        return;
    }
    QLabel *l = m_labels.value(key);
    if (!l)
        return;
    if (key == QLatin1String("clock_hz"))
        l->setText(tr("%1 MHz").arg(v.toDouble() / 1e6, 0, 'f', 1));
    else if (key == QLatin1String("uptime_s"))
        l->setText(uptimeText(v.toLongLong()));
    else if (key == QLatin1String("heap_free"))
        l->setText(tr("%1 B (%2 B)").arg(v.toLongLong()).arg(m_model->value(QStringLiteral("heap_min")).toLongLong()));
    else
        l->setText(v.toString());
    if (key == QLatin1String("heap_min"))
        showValue(QStringLiteral("heap_free"));
}

void Rp2040Page::showStatus()
{
    using S = iwp::DeviceModel::Status;
    const S s = m_model->status();
    if (s == S::Running) {
        m_status->clear();
    } else {
        m_status->setText(m_model->statusMessage());
        if (s == S::Stopped || s == S::Connecting) {
            m_haveRange = false;
            m_range->clear();
        }
    }
}
