#include "fakepsupage.h"

#include <QDial>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLCDNumber>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

FakePsuPage::FakePsuPage(iwp::DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    m_idn = new QLabel(tr("-"));
    m_status = new QLabel;

    m_volt = new QLCDNumber(7);
    m_volt->setSegmentStyle(QLCDNumber::Flat);
    m_volt->setMinimumHeight(70);
    m_temp = new QLabel(QStringLiteral("-"));
    QFont f = m_temp->font();
    f.setPointSize(f.pointSize() * 2);
    m_temp->setFont(f);

    m_spin = new QDoubleSpinBox;
    m_spin->setRange(0.0, 30.0);
    m_spin->setDecimals(2);
    m_spin->setSingleStep(0.1);
    m_spin->setSuffix(tr(" V"));
    m_dial = new QDial;
    m_dial->setRange(0, 300);          // 0.1 V steps
    m_dial->setNotchesVisible(true);
    m_output = new QPushButton(tr("Output"));
    m_output->setCheckable(true);

    auto *meas = new QGroupBox(tr("Measurements"));
    auto *mg = new QGridLayout(meas);
    mg->addWidget(new QLabel(tr("Voltage, V")), 0, 0);
    mg->addWidget(m_volt, 0, 1);
    mg->addWidget(new QLabel(tr("Temperature, C")), 1, 0);
    mg->addWidget(m_temp, 1, 1);
    mg->setColumnStretch(1, 1);

    auto *ctl = new QGroupBox(tr("Control"));
    auto *cg = new QGridLayout(ctl);
    cg->addWidget(new QLabel(tr("Setpoint")), 0, 0);
    cg->addWidget(m_spin, 0, 1);
    cg->addWidget(m_dial, 1, 0, 1, 2);
    cg->addWidget(m_output, 2, 0, 1, 2);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_idn);
    root->addWidget(m_status);
    root->addWidget(meas);
    root->addWidget(ctl);
    root->addStretch(1);

    // debounce: send the setpoint only when the user pauses
    m_sendTimer = new QTimer(this);
    m_sendTimer->setSingleShot(true);
    m_sendTimer->setInterval(150);
    connect(m_sendTimer, &QTimer::timeout, this,
            [this] { m_model->sendCommand(QStringLiteral("setpoint"), m_spin->value()); });

    connect(m_spin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (m_updating)
            return;
        m_updating = true;
        m_dial->setValue(int(std::lround(v * 10.0)));
        m_updating = false;
        m_sendTimer->start();
    });
    connect(m_dial, &QDial::valueChanged, this, [this](int v) {
        if (m_updating)
            return;
        m_updating = true;
        m_spin->setValue(v / 10.0);
        m_updating = false;
        m_sendTimer->start();
    });
    connect(m_output, &QPushButton::clicked, this,
            [this](bool on) { m_model->sendCommand(QStringLiteral("output"), on); });

    connect(m_model, &iwp::DeviceModel::valueChanged, this,
            [this](const QString &key, const QVariant &) { refresh(key); });
    connect(m_model, &iwp::DeviceModel::staleChanged, this,
            [this](const QString &key, bool) { refresh(key); });
    connect(m_model, &iwp::DeviceModel::statusChanged, this, &FakePsuPage::showStatus);

    for (const QString &key : m_model->keys())
        refresh(key);
    showStatus(m_model->status(), m_model->statusMessage());
}

void FakePsuPage::refresh(const QString &key)
{
    const QVariant v = m_model->value(key);
    const bool stale = m_model->isStale(key);

    if (key == QLatin1String("idn")) {
        m_idn->setText(v.toString());
    } else if (key == QLatin1String("volt_meas")) {
        m_volt->display(v.toDouble());
        m_volt->setEnabled(!stale);
    } else if (key == QLatin1String("temp")) {
        m_temp->setText(QString::number(v.toDouble(), 'f', 1));
        m_temp->setEnabled(!stale);
    } else if (key == QLatin1String("setpoint")) {
        // do not fight the user while they are turning the knob
        if (!m_sendTimer->isActive() && !m_spin->hasFocus()) {
            m_updating = true;
            m_spin->setValue(v.toDouble());
            m_dial->setValue(int(std::lround(v.toDouble() * 10.0)));
            m_updating = false;
        }
    } else if (key == QLatin1String("output")) {
        m_output->setChecked(v.toBool());
    }
}

void FakePsuPage::showStatus(iwp::DeviceModel::Status status, const QString &message)
{
    static const char *names[] = {"Stopped", "Connecting", "Starting",
                                  "Running", "Offline", "Error"};
    const bool running = status == iwp::DeviceModel::Status::Running;
    m_status->setText(QLatin1String(names[int(status)])
                      + (message.isEmpty() ? QString() : QStringLiteral(" - ") + message));
    m_status->setStyleSheet(running ? QStringLiteral("color: #2e7d32;")
                                    : QStringLiteral("color: #c62828;"));
    m_spin->setEnabled(running);
    m_dial->setEnabled(running);
    m_output->setEnabled(running);
}
