#include "cansimendpoint.h"

#include <QRegularExpression>
#include <QTimer>

#include "iwp/cancore.h"

namespace {
constexpr int kTickMs = 10;
}

CanSimEndpoint::CanSimEndpoint(QObject *parent) : iwp::CanEndpoint(parent) {}

void CanSimEndpoint::doOpen(const QVariantMap &config)
{
    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        connect(m_timer, &QTimer::timeout, this, &CanSimEndpoint::generate);
    }
    m_echo = config.value("echo", true).toBool();
    m_fd = config.value("fd", false).toBool();
    m_perTick = config.value("rate", 50).toDouble() * kTickMs / 1000.0;
    m_credit = 0;
    m_next = 0;
    m_counter = 0;

    m_templates.clear();
    const QStringList ids = config.value("ids").toString().split(QRegularExpression("[,; ]+"), Qt::SkipEmptyParts);
    for (const QString &text : ids) {
        iwp::can::Frame f;
        std::string error;
        // reuse the frame parser: "<id>#"
        if (!iwp::can::parseFrame((text.trimmed() + "#").toStdString(), f, error)) {
            fail(tr("Bad ID in the generator list: %1").arg(text));
            return;
        }
        m_templates.append(f);
    }
    if (m_perTick > 0 && m_templates.isEmpty()) {
        fail(tr("The generator needs at least one ID"));
        return;
    }
    restartClock();
    m_timer->start(kTickMs);
    setState(State::Online);
}

void CanSimEndpoint::doClose()
{
    if (m_timer)
        m_timer->stop();
}

bool CanSimEndpoint::writeFrame(const iwp::can::Frame &frame, QString *error)
{
    Q_UNUSED(error)
    if (m_echo) {
        iwp::can::Frame back = frame;
        back.flags = quint8(back.flags & ~iwp::can::Tx);
        back.tsUs = 0;   // stamped on delivery
        QMetaObject::invokeMethod(this, [this, back] { deliver(back); }, Qt::QueuedConnection);
    }
    return true;
}

void CanSimEndpoint::generate()
{
    if (m_perTick <= 0 || m_templates.isEmpty())
        return;
    m_credit += m_perTick;
    while (m_credit >= 1.0) {
        m_credit -= 1.0;
        iwp::can::Frame f = m_templates.at(m_next);
        m_next = (m_next + 1) % int(m_templates.size());
        const int len = m_fd ? 16 : 8;
        f.len = quint8(len);
        if (m_fd)
            f.flags |= iwp::can::Fd | iwp::can::Brs;
        for (int i = 0; i < len; ++i)
            f.data[i] = quint8(i < 4 ? (m_counter >> (8 * i)) : (f.id + i));
        ++m_counter;
        deliver(f);
    }
}
