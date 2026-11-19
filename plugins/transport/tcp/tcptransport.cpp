#include "tcptransport.h"

#include <QTcpSocket>

void TcpTransport::ensureSocket()
{
    if (m_socket)
        return;
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, [this] { setState(State::Open); });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        if (state() == State::Open || state() == State::Opening)
            setState(State::Closed);
    });
    connect(m_socket, &QTcpSocket::readyRead, this,
            [this] { emit dataReceived(m_socket->readAll()); });
    connect(m_socket, &QAbstractSocket::errorOccurred, this,
            [this] { fail(m_socket->errorString()); });
}

void TcpTransport::doOpen(const QVariantMap &config)
{
    ensureSocket();
    m_socket->connectToHost(config.value("host", "127.0.0.1").toString(),
                            quint16(config.value("port", 5000).toUInt()));
}

void TcpTransport::doClose()
{
    if (m_socket)
        m_socket->abort();
}

qint64 TcpTransport::doWrite(const QByteArray &data)
{
    return m_socket ? m_socket->write(data) : -1;
}
