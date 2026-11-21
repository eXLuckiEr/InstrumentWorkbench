#include "slcanconfigeditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QWidget>

SlcanConfigEditor::SlcanConfigEditor(QWidget *parent) : iwp::FormConfigEditor(parent)
{
    // Compact and wide: the everyday settings take two lines, the rest is behind "Advanced".
    auto *page = new QWidget;
    auto *grid = new QGridLayout(page);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 3);
    grid->setColumnStretch(3, 1);
    setupPage(page);

    // line 1: port (the list of what is connected now, a name may be typed) and bit rate
    m_port = new QComboBox;
    m_port->setEditable(true);
    m_port->lineEdit()->setPlaceholderText(tr("no ports found"));
    m_port->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *refresh = new QPushButton(tr("Refresh"));
    refresh->setFocusPolicy(Qt::NoFocus);
    auto *portRow = new QHBoxLayout;
    portRow->addWidget(m_port, 1);
    portRow->addWidget(refresh);
    grid->addWidget(new QLabel(tr("Port")), 0, 0);
    grid->addLayout(portRow, 0, 1);
    refreshPorts();
    bindCombo(QStringLiteral("port"), m_port);
    connect(refresh, &QPushButton::clicked, this, &SlcanConfigEditor::refreshPorts);

    auto addCombo = [this](const QString &key, const iwp::ChoiceItems &items) {
        auto *c = new QComboBox;
        for (const auto &it : items)
            c->addItem(it.first, it.second);
        bindCombo(key, c);
        return c;
    };
    auto *bitrate = addCombo(QStringLiteral("bitrate"),
                             {{tr("10 kbit/s"), 10000}, {tr("20 kbit/s"), 20000}, {tr("50 kbit/s"), 50000},
                              {tr("100 kbit/s"), 100000}, {tr("125 kbit/s"), 125000}, {tr("250 kbit/s"), 250000},
                              {tr("500 kbit/s"), 500000}, {tr("1 Mbit/s"), 1000000}});
    grid->addWidget(new QLabel(tr("Bit rate")), 0, 2);
    grid->addWidget(bitrate, 0, 3);

    // line 2: CAN FD, its data rate and the "Advanced" switch
    auto *fd = new QCheckBox(tr("CAN FD"));
    bindFlag(QStringLiteral("fd"), fd);
    auto *dataRate = addCombo(QStringLiteral("dataBitrate"),
                              {{tr("1 Mbit/s"), 1000000}, {tr("2 Mbit/s"), 2000000}, {tr("4 Mbit/s"), 4000000},
                               {tr("5 Mbit/s"), 5000000}});
    dataRate->setEnabled(false);
    connect(fd, &QCheckBox::toggled, dataRate, &QWidget::setEnabled);
    m_advancedButton = new QPushButton;
    m_advancedButton->setCheckable(true);
    m_advancedButton->setFlat(true);
    m_advancedButton->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(fd, 1, 0);
    auto *dataRow = new QHBoxLayout;
    dataRow->addWidget(new QLabel(tr("FD data rate")));
    dataRow->addWidget(dataRate, 1);
    grid->addLayout(dataRow, 1, 1);
    grid->addWidget(m_advancedButton, 1, 3, Qt::AlignRight);

    // advanced options, hidden by default
    m_advanced = new QWidget;
    auto *adv = new QGridLayout(m_advanced);
    adv->setContentsMargins(0, 0, 0, 0);
    adv->setColumnStretch(2, 1);
    auto *listen = new QCheckBox(tr("Listen only (no acknowledge, no transmit)"));
    bindFlag(QStringLiteral("listenOnly"), listen);
    auto *retry = new QCheckBox(tr("Automatic retransmission"));
    bindFlag(QStringLiteral("autoRetransmit"), retry);
    auto *extra = new QLineEdit;
    extra->setPlaceholderText(tr("adapter commands before opening, separated by ;"));
    bindText(QStringLiteral("initCommands"), extra);
    adv->addWidget(listen, 0, 0, 1, 2);
    adv->addWidget(retry, 0, 2);
    adv->addWidget(new QLabel(tr("Extra commands")), 1, 0);
    adv->addWidget(extra, 1, 1, 1, 2);
    grid->addWidget(m_advanced, 2, 0, 1, 4);
    setAdvanced(false);
    connect(m_advancedButton, &QPushButton::toggled, this, &SlcanConfigEditor::setAdvanced);
}

void SlcanConfigEditor::setAdvanced(bool on)
{
    m_advanced->setVisible(on);
    m_advancedButton->setText(on ? tr("Advanced ▲") : tr("Advanced ▼"));
    QSignalBlocker block(m_advancedButton);
    m_advancedButton->setChecked(on);
}

void SlcanConfigEditor::setConfig(const QVariantMap &config)
{
    iwp::FormConfigEditor::setConfig(config);
    // unusual values must not hide: show the advanced block when something in it is not the default
    const bool unusual = config.value(QStringLiteral("listenOnly")).toBool()
                         || !config.value(QStringLiteral("autoRetransmit"), true).toBool()
                         || !config.value(QStringLiteral("initCommands")).toString().trimmed().isEmpty();
    if (unusual)
        setAdvanced(true);
}

void SlcanConfigEditor::refreshPorts()
{
    const int i = m_port->currentIndex();
    const QString text = m_port->currentText().trimmed();
    const QString keep = (i >= 0 && text == m_port->itemText(i)) ? m_port->itemData(i).toString() : text;

    m_port->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
#ifdef Q_OS_MACOS
        if (info.systemLocation().startsWith(QLatin1String("/dev/tty.")))
            continue;   // /dev/cu.* is the right twin for outgoing connections
#endif
        const QString description = info.description();
        m_port->addItem(description.isEmpty() ? info.systemLocation()
                                              : info.systemLocation() + QStringLiteral(" - ") + description,
                        info.systemLocation());
    }
    if (keep.isEmpty())
        return;
    const int idx = m_port->findData(keep);
    if (idx >= 0)
        m_port->setCurrentIndex(idx);
    else
        m_port->setEditText(keep);
}

QString SlcanConfigEditor::validate() const
{
    return config().value(QStringLiteral("port")).toString().trimmed().isEmpty()
               ? tr("Select the adapter's serial port") : QString();
}
