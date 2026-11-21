#pragma once
#include <QList>

#include "iwp/canendpoint.h"

class QTimer;

class CanSimEndpoint : public iwp::CanEndpoint
{
    Q_OBJECT
public:
    explicit CanSimEndpoint(QObject *parent = nullptr);

protected:
    void doOpen(const QVariantMap &config) override;
    void doClose() override;
    bool writeFrame(const iwp::can::Frame &frame, QString *error) override;

private:
    void generate();

    QTimer *m_timer = nullptr;
    bool m_echo = true;
    bool m_fd = false;
    double m_perTick = 0;
    double m_credit = 0;
    QList<iwp::can::Frame> m_templates;
    int m_next = 0;
    quint32 m_counter = 0;
};
