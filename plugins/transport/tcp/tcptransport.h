#pragma once
#include "iwp/itransport.h"

class QTcpSocket;

class TcpTransport : public iwp::ITransport
{
    Q_OBJECT
public:
    using iwp::ITransport::ITransport;
    QString typeId() const override { return QStringLiteral("tcp"); }

protected:
    void doOpen(const QVariantMap &config) override;
    void doClose() override;
    qint64 doWrite(const QByteArray &data) override;

private:
    void ensureSocket();   // socket is created in the worker thread
    QTcpSocket *m_socket = nullptr;
};
