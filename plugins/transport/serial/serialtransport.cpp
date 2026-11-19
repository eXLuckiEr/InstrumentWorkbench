#include "serialtransport.h"

#include <QSerialPortInfo>

namespace {

QSerialPort::Parity parityFrom(const QString &s)
{
    const QString v = s.toLower();
    if (v == QLatin1String("even"))  return QSerialPort::EvenParity;
    if (v == QLatin1String("odd"))   return QSerialPort::OddParity;
    if (v == QLatin1String("space")) return QSerialPort::SpaceParity;
    if (v == QLatin1String("mark"))  return QSerialPort::MarkParity;
    return QSerialPort::NoParity;
}

QSerialPort::StopBits stopBitsFrom(const QString &s)
{
    if (s == QLatin1String("1.5")) return QSerialPort::OneAndHalfStop;
    if (s == QLatin1String("2"))   return QSerialPort::TwoStop;
    return QSerialPort::OneStop;
}

QSerialPort::DataBits dataBitsFrom(int n)
{
    switch (n) {
    case 5:  return QSerialPort::Data5;
    case 6:  return QSerialPort::Data6;
    case 7:  return QSerialPort::Data7;
    default: return QSerialPort::Data8;
    }
}

QSerialPort::FlowControl flowFrom(const QString &s)
{
    const QString v = s.toLower();
    if (v == QLatin1String("hardware")) return QSerialPort::HardwareControl;
    if (v == QLatin1String("software")) return QSerialPort::SoftwareControl;
    return QSerialPort::NoFlowControl;
}

} // namespace

QString SerialTransport::availablePortsText()
{
    QStringList names;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        names << (info.description().isEmpty()
                      ? info.systemLocation()
                      : QStringLiteral("%1 (%2)").arg(info.systemLocation(), info.description()));
    return names.isEmpty() ? tr("none found") : names.join(QStringLiteral(", "));
}

void SerialTransport::ensurePort()
{
    if (m_port)
        return;
    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead, this,
            [this] { emit dataReceived(m_port->readAll()); });
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialTransport::onPortError);
}

void SerialTransport::doOpen(const QVariantMap &config)
{
    ensurePort();
    if (m_port->isOpen())
        m_port->close();

    const QString name = config.value("port").toString();
    if (name.isEmpty()) {
        fail(tr("No port specified. Available: %1").arg(availablePortsText()));
        return;
    }

    const QSerialPort::FlowControl flow = flowFrom(config.value("flow", "none").toString());
    m_port->setPortName(name);
    m_port->setBaudRate(config.value("baud", 115200).toInt());
    m_port->setDataBits(dataBitsFrom(config.value("dataBits", 8).toInt()));
    m_port->setParity(parityFrom(config.value("parity", "none").toString()));
    m_port->setStopBits(stopBitsFrom(config.value("stopBits", "1").toString()));
    m_port->setFlowControl(flow);

    if (!m_port->open(QIODevice::ReadWrite)) {
        fail(tr("Cannot open %1: %2. Available: %3")
                 .arg(name, m_port->errorString(), availablePortsText()));
        return;
    }
    if (config.contains("dtr"))
        m_port->setDataTerminalReady(config.value("dtr").toBool());
    if (config.contains("rts") && flow != QSerialPort::HardwareControl)
        m_port->setRequestToSend(config.value("rts").toBool());

    m_port->clear();   // drop whatever was left in the driver buffers
    setState(State::Open);
}

void SerialTransport::doClose()
{
    if (m_port && m_port->isOpen())
        m_port->close();
}

qint64 SerialTransport::doWrite(const QByteArray &data)
{
    return m_port ? m_port->write(data) : -1;
}

void SerialTransport::onPortError(QSerialPort::SerialPortError error)
{
    switch (error) {
    case QSerialPort::NoError:
        return;
    case QSerialPort::DeviceNotFoundError:
    case QSerialPort::PermissionError:
    case QSerialPort::OpenError:
    case QSerialPort::NotOpenError:
    case QSerialPort::UnsupportedOperationError:
    case QSerialPort::ReadError:
    case QSerialPort::WriteError:
    case QSerialPort::ResourceError:   // device unplugged, driver gone...
        if (m_port->isOpen())
            m_port->close();
        fail(m_port->errorString());
        return;
    default:
        // Anything else (timeouts, line noise reported by some drivers) is not
        // fatal: the framer resynchronises. Report rarely.
        if (!m_noiseClock.isValid() || m_noiseClock.elapsed() > 1000) {
            m_noiseClock.restart();
            emit errorOccurred(m_port->errorString());
        }
        return;
    }
}
