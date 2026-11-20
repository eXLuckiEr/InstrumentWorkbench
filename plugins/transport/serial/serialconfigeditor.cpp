#include "serialconfigeditor.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSerialPortInfo>

SerialConfigEditor::SerialConfigEditor(QWidget *parent) : iwp::FormConfigEditor(parent)
{
    m_port = new QComboBox;
    m_port->setEditable(true);                       // a port that is not listed can be typed
    m_port->setInsertPolicy(QComboBox::NoInsert);
    m_port->setMinimumContentsLength(26);
    m_port->lineEdit()->setPlaceholderText(tr("no ports found"));
    auto *refresh = new QPushButton(tr("Refresh"));
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_port, 1);
    layout->addWidget(refresh);
    form()->addRow(tr("Port"), row);
    refreshPorts();
    bindCombo(QStringLiteral("port"), m_port);
    connect(refresh, &QPushButton::clicked, this, &SerialConfigEditor::refreshPorts);

    iwp::ChoiceItems bauds;
    for (int baud : {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000})
        bauds.append({QString::number(baud), baud});
    addChoice(QStringLiteral("baud"), tr("Baud rate"), bauds, true);

    iwp::ChoiceItems bits;
    for (int n : {5, 6, 7, 8})
        bits.append({QString::number(n), n});
    addChoice(QStringLiteral("dataBits"), tr("Data bits"), bits);

    addChoice(QStringLiteral("parity"), tr("Parity"),
              {{tr("None"), QStringLiteral("none")}, {tr("Even"), QStringLiteral("even")},
               {tr("Odd"), QStringLiteral("odd")}, {tr("Space"), QStringLiteral("space")},
               {tr("Mark"), QStringLiteral("mark")}});
    addChoice(QStringLiteral("stopBits"), tr("Stop bits"),
              {{QStringLiteral("1"), QStringLiteral("1")},
               {tr("1.5 (Windows only)"), QStringLiteral("1.5")},
               {QStringLiteral("2"), QStringLiteral("2")}});
    addChoice(QStringLiteral("flow"), tr("Flow control"),
              {{tr("None"), QStringLiteral("none")},
               {tr("Hardware (RTS/CTS)"), QStringLiteral("hardware")},
               {tr("Software (XON/XOFF)"), QStringLiteral("software")}});
}

void SerialConfigEditor::refreshPorts()
{
    // what is selected or typed now (the port may have been unplugged meanwhile)
    const int i = m_port->currentIndex();
    const QString text = m_port->currentText().trimmed();
    const QString keep = (i >= 0 && text == m_port->itemText(i)) ? m_port->itemData(i).toString() : text;

    m_port->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
#ifdef Q_OS_MACOS
        if (info.systemLocation().startsWith(QLatin1String("/dev/tty.")))
            continue;   // on macOS the /dev/cu.* twin is the right one for outgoing connections
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

QString SerialConfigEditor::validate() const
{
    return config().value(QStringLiteral("port")).toString().trimmed().isEmpty()
               ? tr("Select a serial port") : QString();
}
