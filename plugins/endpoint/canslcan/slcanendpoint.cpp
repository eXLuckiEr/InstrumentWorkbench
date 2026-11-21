#include "slcanendpoint.h"

#include <QDateTime>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>

#include "slcancodec.h"

namespace {
constexpr int kAckTimeoutMs = 300;   // this firmware may not acknowledge commands at all
}

SlcanEndpoint::SlcanEndpoint(QObject *parent) : iwp::CanEndpoint(parent) {}
SlcanEndpoint::~SlcanEndpoint() = default;

void SlcanEndpoint::doOpen(const QVariantMap &config)
{
    if (!m_port) {   // created here, not in the constructor: the endpoint is moved to its thread
        m_port = new QSerialPort(this);
        connect(m_port, &QSerialPort::readyRead, this, &SlcanEndpoint::onReadyRead);
        connect(m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
            if (e == QSerialPort::NoError)
                return;
            if (e == QSerialPort::ResourceError || e == QSerialPort::DeviceNotFoundError
                || e == QSerialPort::PermissionError || e == QSerialPort::ReadError
                || e == QSerialPort::WriteError) {
                m_opening = false;
                if (m_ackTimer)
                    m_ackTimer->stop();
                if (m_port->isOpen())
                    m_port->close();
                fail(m_port->errorString());
            }
        });
        m_ackTimer = new QTimer(this);
        m_ackTimer->setSingleShot(true);
        connect(m_ackTimer, &QTimer::timeout, this, [this] {
            // No answer is not an error by itself: the CANable 2 firmware does not acknowledge
            // every command. Only silence during the whole opening means "not an SLCAN adapter".
            if (m_opening && !m_steps.isEmpty())
                nextStep();
        });
    }
    if (m_port->isOpen())
        m_port->close();

    const QString name = config.value("port").toString();
    if (name.isEmpty()) {
        QStringList names;
        for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
            names << info.systemLocation();
        fail(tr("No port specified. Available: %1").arg(names.isEmpty() ? tr("none found") : names.join(", ")));
        return;
    }

    const int bitrate = config.value("bitrate", 500000).toInt();
    const char nominal = iwp::can::slcan::nominalCode(bitrate);
    if (!nominal) {
        fail(tr("Bit rate %1 is not supported by the adapter presets (10k, 20k, 50k, 100k, 125k, 250k, 500k, 1M); "
                "use \"initCommands\" for anything else").arg(bitrate));
        return;
    }
    const bool fd = config.value("fd", false).toBool();
    const char dataCode = iwp::can::slcan::dataCode(config.value("dataBitrate", 2000000).toInt());
    if (fd && !dataCode) {
        fail(tr("FD data bit rate must be 1, 2, 4 or 5 Mbit/s"));
        return;
    }

    m_port->setPortName(name);
    m_port->setBaudRate(config.value("baud", 115200).toInt());
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);
    if (!m_port->open(QIODevice::ReadWrite)) {
        fail(tr("Cannot open %1: %2").arg(name, m_port->errorString()));
        return;
    }
    m_port->clear();
    m_buffer.clear();

    m_heard = false;
    m_steps.clear();
    m_steps.append({"V\r", false, QStringLiteral("V")});   // version: proves that SLCAN is on the other side
    m_steps.append({"C\r", false, QStringLiteral("C")});   // close: it may be open from an earlier session
    m_steps.append({QByteArray("S") + nominal + "\r", true, tr("bit rate")});
    if (fd)
        m_steps.append({QByteArray("Y") + dataCode + "\r", true, tr("FD data bit rate")});
    if (config.value("listenOnly", false).toBool())
        m_steps.append({"M1\r", true, tr("listen-only mode")});
    if (!config.value("autoRetransmit", true).toBool())
        m_steps.append({"A0\r", false, tr("automatic retransmission")});
    const QStringList extra = config.value("initCommands").toString().split(';', Qt::SkipEmptyParts);
    for (const QString &c : extra)
        m_steps.append({c.trimmed().toLatin1() + "\r", false, c.trimmed()});
    m_steps.append({"O\r", true, tr("opening the channel")});

    m_opening = true;
    m_stepSent = false;
    nextStep();
}

void SlcanEndpoint::nextStep()
{
    if (m_stepSent && !m_steps.isEmpty())
        m_steps.removeFirst();
    m_stepSent = false;
    if (m_steps.isEmpty()) {
        if (!m_heard) {
            abortOpening(tr("The adapter does not answer. Is it in SLCAN mode (not gs_usb/candlelight)?"));
            return;
        }
        m_opening = false;
        restartClock();
        setState(State::Online);
        return;
    }
    m_stepSent = true;
    m_port->write(m_steps.first().command);
    m_ackTimer->start(kAckTimeoutMs);
}

void SlcanEndpoint::abortOpening(const QString &message)
{
    m_opening = false;
    m_ackTimer->stop();
    if (m_port && m_port->isOpen())
        m_port->close();
    fail(message);
}

void SlcanEndpoint::doClose()
{
    m_opening = false;
    if (m_ackTimer)
        m_ackTimer->stop();
    if (m_port && m_port->isOpen()) {
        m_port->write("C\r");
        m_port->waitForBytesWritten(100);
        m_port->close();
    }
}

bool SlcanEndpoint::writeFrame(const iwp::can::Frame &frame, QString *error)
{
    if (!m_port || !m_port->isOpen() || m_opening) {
        *error = tr("The adapter is not open");
        return false;
    }
    if (frame.has(iwp::can::Fd) && !config().value("fd", false).toBool()) {
        *error = tr("CAN FD frame, but FD is off in the connection settings");
        return false;
    }
    const std::string text = iwp::can::slcan::encode(frame);
    if (text.empty()) {
        *error = tr("This frame cannot be sent (length does not fit the frame type)");
        return false;
    }
    if (m_port->write(text.data(), qint64(text.size())) != qint64(text.size())) {
        *error = m_port->errorString();
        return false;
    }
    return true;
}

void SlcanEndpoint::onReadyRead()
{
    const QByteArray chunk = m_port->readAll();
    if (m_opening && !chunk.isEmpty())
        m_heard = true;
    for (char c : chunk) {
        if (c == '\r') {
            if (m_buffer.isEmpty())
                onAck(true);   // plain '\r': the answer to a command
            else {
                onLine(m_buffer);
                m_buffer.clear();
            }
        } else if (c == '\a') {
            m_buffer.clear();
            onAck(false);
        } else if (c != '\n') {
            if (m_buffer.size() < 512)
                m_buffer.append(c);
        }
    }
}

void SlcanEndpoint::onLine(const QByteArray &line)
{
    iwp::can::Frame f;
    if (iwp::can::slcan::decode(std::string(line.constData(), size_t(line.size())), f)) {
        deliver(f);
        return;
    }
    // "z" / "Z": the adapter confirms a transmitted frame (and some commands)
    if (line == "z" || line == "Z")
        onAck(true);
    else if (m_opening)
        onAck(true);   // the reply to "V" (version text) while opening
    // anything else is of no interest
}

void SlcanEndpoint::onAck(bool ok)
{
    if (!m_opening) {
        if (!ok) {   // rejected command / frame during work; do not flood the log
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - m_lastWarnMs > 2000) {
                m_lastWarnMs = now;
                emit errorOccurred(tr("The adapter rejected a command or frame (BEL)"));
            }
        }
        return;
    }
    if (!m_stepSent || m_steps.isEmpty())
        return;
    m_ackTimer->stop();
    const Step step = m_steps.first();
    if (!ok && step.required) {
        abortOpening(tr("The adapter rejected: %1").arg(step.what));
        return;
    }
    if (!ok)
        emit errorOccurred(tr("The adapter rejected the optional command: %1").arg(step.what));
    nextStep();
}
