#include "mkssession.h"

#include <QSharedPointer>
#include <QTimer>
#include <algorithm>

namespace {
constexpr int kPollMs = 100;
constexpr int kPublishMs = 100;
constexpr int kPollTimeoutMs = 80;
constexpr int kOfflineAfter = 3;     // missed answers before a motor is shown offline
constexpr int kMaxQueued = 12;       // do not pile up polls
}

MksSession::MksSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent)
    : SessionBase(endpoints, model, parent), m_pollTimer(new QTimer(this)), m_publishTimer(new QTimer(this)),
      m_replyTimer(new QTimer(this))
{
    qRegisterMetaType<iwp::can::Frame>("iwp::can::Frame");
    m_pollTimer->setInterval(kPollMs);
    connect(m_pollTimer, &QTimer::timeout, this, &MksSession::pollTick);
    m_publishTimer->setInterval(kPublishMs);
    connect(m_publishTimer, &QTimer::timeout, this, &MksSession::publish);
    m_replyTimer->setSingleShot(true);
    connect(m_replyTimer, &QTimer::timeout, this, &MksSession::onTimeout);

    m_bus = qobject_cast<iwp::CanEndpoint *>(endpoint(QStringLiteral("bus")));
    if (m_bus)
        connect(m_bus, &iwp::CanEndpoint::frameReceived, this, &MksSession::onFrame);
    m_clock.start();
}

void MksSession::onStart()
{
    QString label;
    if (m_bus) {
        const QVariantMap cfg = m_bus->config();
        const QString port = cfg.value("port").toString();
        label = port.isEmpty() ? tr("simulator") : port;
        label += tr(", %1 kbit/s").arg(cfg.value("bitrate", 500000).toDouble() / 1000.0, 0, 'f', 0);
    }
    model()->update(QStringLiteral("connection"), label);
    m_busy = false;
    m_queue.clear();
    m_pollTimer->start();
    m_publishTimer->start();
    m_dirty = true;
    publish();
    setRunning();
}

void MksSession::onStopped()
{
    m_pollTimer->stop();
    m_publishTimer->stop();
    m_replyTimer->stop();
    m_queue.clear();
    m_busy = false;
}

MksSession::Motor &MksSession::motor(int id)
{
    Motor &m = m_motors[id];
    m.id = id;
    return m;
}

void MksSession::log(const QString &text)
{
    model()->postEvent(QStringLiteral("log"), text);
}

// ---------------------------------------------------------------- the exchange

void MksSession::enqueue(Pending p)
{
    if (p.replyCode < 0)
        p.replyCode = (p.code == mks::code::ReadConfig && !p.params.empty()) ? p.params.front() : p.code;
    auto it = m_queue.begin();
    while (it != m_queue.end() && it->priority >= p.priority)
        ++it;
    m_queue.insert(it, std::move(p));
    pump();
}

void MksSession::pump()
{
    if (m_busy || m_queue.isEmpty())
        return;
    m_cur = m_queue.takeFirst();
    m_busy = true;
    sendCurrent();
}

void MksSession::sendCurrent()
{
    if (!m_bus) {
        finishCurrent(false, nullptr);
        return;
    }
    const iwp::can::Frame f = mks::request(quint16(m_cur.id), m_cur.code, m_cur.params);
    if (m_cur.logIt || m_verbose)
        log(QStringLiteral("→ %1: %2").arg(m_cur.id)
                .arg(QString::fromStdString(iwp::can::toHex(f.data, f.len, true))));
    const bool wait = m_cur.expectReply;
    // the answer can come before the handler of sendFrame runs, so the timer is started first
    if (wait)
        m_replyTimer->start(m_cur.timeoutMs);
    m_bus->sendFrame(f, [this, wait](const iwp::Response &r) {
        if (!r.ok) {
            m_replyTimer->stop();
            log(tr("Send failed: %1").arg(r.error));
            finishCurrent(false, nullptr);
        } else if (!wait) {
            // no answer expected (broadcast): leave a moment to the bus
            QTimer::singleShot(5, this, [this] { finishCurrent(true, nullptr); });
        }
    });
}

void MksSession::finishCurrent(bool ok, const iwp::can::Frame *frame)
{
    if (!m_busy)
        return;
    m_replyTimer->stop();
    const Pending cur = m_cur;
    m_busy = false;
    if (cur.done)
        cur.done(ok, frame);
    pump();
}

void MksSession::onTimeout()
{
    if (!m_busy)
        return;
    if (m_cur.logIt || m_verbose)
        log(tr("No answer from motor %1 to command %2")
                .arg(m_cur.id)
                .arg(QStringLiteral("%1").arg(m_cur.code, 2, 16, QLatin1Char('0')).toUpper()));
    finishCurrent(false, nullptr);
}

void MksSession::onFrame(const iwp::can::Frame &frame)
{
    if (frame.tx())
        return;
    quint8 cmd = 0;
    const quint8 *p = nullptr;
    int n = 0;
    if (!mks::parseResponse(frame, mks::CrcMode::Sum8, cmd, p, n)) {
        if (m_verbose)
            log(tr("Frame %1: not a valid MKS answer (CRC)").arg(QString::fromStdString(iwp::can::formatFrame(frame))));
        return;
    }
    const int id = int(frame.id);
    Motor &m = motor(id);
    const bool wasOnline = m.online;
    m.online = true;
    m.fails = 0;
    if (!wasOnline && !m.haveVersion) {
        Pending v;
        v.id = id;
        v.code = mks::code::Version;
        v.priority = 3;
        v.timeoutMs = kPollTimeoutMs;
        enqueue(v);
    }
    applyReply(m, cmd, p, n);
    m_dirty = true;

    const bool matches = m_busy && m_cur.expectReply && m_cur.id == id && m_cur.replyCode == cmd;
    if (matches || m_verbose || (m_busy && m_cur.id == id && m_cur.logIt))
        log(QStringLiteral("← %1: %2").arg(id).arg(describe(cmd, p, n)));
    else if (!matches)
        log(QStringLiteral("← %1: %2").arg(id).arg(describe(cmd, p, n)));   // an unexpected answer is interesting
    if (matches)
        finishCurrent(true, &frame);
}

// ---------------------------------------------------------------- replies

QString MksSession::describe(int cmd, const quint8 *p, int n)
{
    const QString head = QStringLiteral("%1").arg(cmd, 2, 16, QLatin1Char('0')).toUpper();
    switch (cmd) {
    case mks::code::Encoder: {
        mks::EncoderValue e;
        if (mks::decodeEncoder(p, n, e))
            return QStringLiteral("%1 carry=%2 value=%3 (%4°)").arg(head).arg(e.carry).arg(e.value).arg(e.degrees(), 0, 'f', 2);
        break;
    }
    case mks::code::EncoderAccum: {
        std::int64_t v;
        if (mks::decodeI48(p, n, v))
            return QStringLiteral("%1 %2 counts (%3°)").arg(head).arg(v).arg(double(v) * 360.0 / 16384.0, 0, 'f', 2);
        break;
    }
    case mks::code::Speed: {
        int rpm;
        if (mks::decodeSpeed(p, n, rpm))
            return QStringLiteral("%1 %2 rpm").arg(head).arg(rpm);
        break;
    }
    case mks::code::IoStatus: {
        mks::IoState io;
        if (mks::decodeIo(p, n, io))
            return QStringLiteral("%1 IN_1=%2 IN_2=%3 OUT_1=%4 OUT_2=%5").arg(head).arg(io.in1).arg(io.in2).arg(io.out1).arg(io.out2);
        break;
    }
    case mks::code::MotorStatus:
        if (n >= 1)
            return QStringLiteral("%1 %2").arg(head, QString::fromLatin1(mks::runStatusName(p[0])));
        break;
    case mks::code::VoltCurrent: {
        mks::VoltCurrent vc;
        if (mks::decodeVoltCurrent(p, n, vc))
            return QStringLiteral("%1 %2 V, %3 mA").arg(head).arg(vc.millivolts / 1000.0, 0, 'f', 2).arg(vc.milliamps);
        break;
    }
    case mks::code::HomeState:
        if (n >= 2)
            return QStringLiteral("%1 single-turn: %2, other: %3")
                .arg(head, QString::fromLatin1(mks::homeStateName(p[0])), QString::fromLatin1(mks::homeStateName(p[1])));
        break;
    case mks::code::Version: {
        mks::VersionInfo v;
        if (mks::decodeVersion(p, n, v))
            return QStringLiteral("%1 %2, firmware 0x%3, %4").arg(head, QString::fromLatin1(mks::hardwareName(v.hardware)))
                .arg(v.firmware, 0, 16).arg(v.calibrated ? QStringLiteral("calibrated") : QStringLiteral("not calibrated"));
        break;
    }
    case mks::code::HomeParams: {
        mks::HomeParams h;
        if (mks::decodeHomeParams(p, n, h))
            return QStringLiteral("%1 trigger=%2 dir=%3 speed=%4 limit=%5 mode=%6")
                .arg(head).arg(h.trigger).arg(h.direction).arg(h.speedRpm).arg(h.endLimit).arg(h.mode);
        break;
    }
    default:
        break;
    }
    return head + QLatin1Char(' ') + QString::fromStdString(iwp::can::toHex(p, n, true));
}

void MksSession::applyReply(Motor &m, int cmd, const quint8 *p, int n)
{
    switch (cmd) {
    case mks::code::EncoderAccum: {
        std::int64_t v;
        if (mks::decodeI48(p, n, v)) { m.posCounts = v; m.havePos = true; }
        break;
    }
    case mks::code::Encoder: {
        mks::EncoderValue e;
        if (mks::decodeEncoder(p, n, e)) { m.posCounts = e.total(); m.havePos = true; }
        break;
    }
    case mks::code::Speed: {
        int rpm;
        if (mks::decodeSpeed(p, n, rpm)) m.speed = rpm;
        break;
    }
    case mks::code::IoStatus:
        if (mks::decodeIo(p, n, m.io)) m.haveIo = true;
        break;
    case mks::code::MotorStatus:
        if (n >= 1) m.runStatus = p[0];
        break;
    case mks::code::StallState:
        if (n >= 1) m.stall = p[0];
        break;
    case mks::code::EnableState:
        if (n >= 1) m.enabled = p[0];
        break;
    case mks::code::VoltCurrent:
        if (mks::decodeVoltCurrent(p, n, m.vi)) m.haveVi = true;
        break;
    case mks::code::AngleError: {
        double d;
        if (mks::decodeAngleError(p, n, d)) m.angleError = d;
        break;
    }
    case mks::code::HomeState:
        if (n >= 2) { m.home1 = p[0]; m.home2 = p[1]; }
        break;
    case mks::code::Version:
        if (mks::decodeVersion(p, n, m.version)) m.haveVersion = true;
        break;
    case mks::code::GoHome:
        if (n >= 1) {
            m.note = p[0] == 1 ? tr("homing started") : (p[0] == 2 ? tr("homing done") : tr("homing failed"));
            if (p[0] == 2) m.home1 = 1;
        }
        break;
    case mks::code::MoveRel:
        if (n >= 1) {
            if (p[0] == 2)
                m.note = tr("move complete");
            else if (p[0] == 3)
                m.note = tr("stopped by a limit switch");
            else if (p[0] == 0)
                m.note = tr("move failed");
        }
        break;
    case mks::code::SpeedRun:
        if (n >= 1) {
            if (p[0] == 3)
                m.note = tr("stopped by a limit switch");
            else if (p[0] == 2)
                m.note = tr("run finished");
        }
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- polling

void MksSession::pollTick()
{
    if (!m_bus || m_queue.size() > kMaxQueued)
        return;
    ++m_tick;
    auto poll = [this](int id, quint8 code) {
        for (const Pending &q : std::as_const(m_queue))
            if (q.id == id && q.code == code && q.priority == 0)
                return;   // still waiting
        if (m_busy && m_cur.id == id && m_cur.code == code)
            return;
        Pending p;
        p.id = id;
        p.code = code;
        p.priority = 0;
        p.timeoutMs = kPollTimeoutMs;
        p.done = [this, id](bool ok, const iwp::can::Frame *) {
            Motor &m = motor(id);
            if (ok)
                return;
            if (m.online && ++m.fails >= kOfflineAfter) {
                m.online = false;
                m.haveIo = false;
                m_dirty = true;
            }
        };
        enqueue(p);
    };

    for (int id : std::as_const(m_ids)) {
        Motor &m = motor(id);
        if (!m.online) {
            if (m_tick % 20 == 0)
                poll(id, mks::code::Version);
            continue;
        }
        poll(id, mks::code::IoStatus);                         // the endstops: fast, for every motor
        if (id == m_selected) {
            if (m_tick % 2 == 0) {
                poll(id, mks::code::EncoderAccum);
                poll(id, mks::code::Speed);
                poll(id, mks::code::MotorStatus);
            }
            if (m_tick % 5 == 0)
                poll(id, mks::code::StallState);
            if (m_tick % 10 == 0) {
                poll(id, mks::code::VoltCurrent);
                poll(id, mks::code::AngleError);
                poll(id, mks::code::EnableState);
                poll(id, mks::code::HomeState);
            }
        } else if (m_tick % 5 == 0) {
            poll(id, mks::code::EncoderAccum);
            poll(id, mks::code::MotorStatus);
        }
    }
}

void MksSession::publish()
{
    if (!m_dirty)
        return;
    m_dirty = false;
    QVariantList list;
    for (int id : std::as_const(m_ids)) {
        const Motor &m = m_motors.value(id);
        QVariantMap v;
        v["id"] = id;
        v["online"] = m.online;
        v["haveVersion"] = m.haveVersion;
        if (m.haveVersion) {
            v["hardware"] = QString::fromLatin1(mks::hardwareName(m.version.hardware));
            v["firmware"] = m.version.firmware;
            v["calibrated"] = m.version.calibrated;
        }
        v["havePos"] = m.havePos;
        v["pos"] = double(m.posCounts);
        v["speed"] = m.speed;
        v["runStatus"] = m.runStatus;
        v["haveIo"] = m.haveIo;
        v["in1"] = m.io.in1;
        v["in2"] = m.io.in2;
        v["out1"] = m.io.out1;
        v["out2"] = m.io.out2;
        v["ioRaw"] = int(m.io.raw);
        v["stall"] = m.stall;
        v["enabled"] = m.enabled;
        v["haveVi"] = m.haveVi;
        v["mv"] = m.vi.millivolts;
        v["ma"] = m.vi.milliamps;
        v["angleError"] = m.angleError;
        v["home1"] = m.home1;
        v["home2"] = m.home2;
        v["note"] = m.note;
        list.append(v);
    }
    model()->update(QStringLiteral("motors"), list, QStringLiteral("bus"));
}

// ---------------------------------------------------------------- commands from the page

void MksSession::onCommand(const QString &name, const QVariant &arg)
{
    if (name == QLatin1String("setIds")) {
        QList<int> ids;
        for (const QVariant &v : arg.toList()) {
            const int id = v.toInt();
            if (id >= 1 && id <= 2047 && !ids.contains(id))
                ids.append(id);
        }
        std::sort(ids.begin(), ids.end());
        m_ids = ids;
        for (int id : std::as_const(m_ids))
            motor(id);
        m_dirty = true;
        publish();
    } else if (name == QLatin1String("select")) {
        m_selected = arg.toInt();
    } else if (name == QLatin1String("verbose")) {
        m_verbose = arg.toBool();
    } else if (name == QLatin1String("scan")) {
        const QVariantMap m = arg.toMap();
        scan(m.value("from", 1).toInt(), m.value("to", 16).toInt());
    } else if (name == QLatin1String("readConfig")) {
        readConfig(arg.toInt());
    } else if (name == QLatin1String("estopAll")) {
        // broadcast: nobody answers, but everybody stops
        Pending p;
        p.id = 0;
        p.code = mks::code::EmergencyStop;
        p.params = {0x01};
        p.priority = 100;
        p.expectReply = false;
        p.logIt = true;
        enqueue(p);
        log(tr("Emergency stop sent to all motors (broadcast)"));
    } else if (name == QLatin1String("request")) {
        const QVariantMap m = arg.toMap();
        Pending p;
        p.id = m.value("id").toInt();
        p.code = quint8(m.value("code").toInt());
        const QByteArray params = m.value("params").toByteArray();
        for (char c : params)
            p.params.push_back(quint8(c));
        p.replyCode = m.value("reply", -1).toInt();
        p.timeoutMs = m.value("timeout", 300).toInt();
        p.priority = 10;
        p.logIt = true;
        p.expectReply = p.id != 0 && m.value("expectReply", true).toBool();
        const QString note = m.value("note").toString();
        const int id = p.id;
        const int code = p.code;
        p.done = [this, id, code, note](bool ok, const iwp::can::Frame *frame) {
            QVariantMap r;
            r["id"] = id;
            r["code"] = code;
            r["ok"] = ok;
            r["note"] = note;
            r["status"] = -1;
            QString text = ok ? tr("ok") : tr("no answer");
            if (ok && frame) {
                quint8 cmd;
                const quint8 *pp;
                int n;
                if (mks::parseResponse(*frame, mks::CrcMode::Sum8, cmd, pp, n) && n >= 1) {
                    r["status"] = pp[0];
                    r["params"] = QByteArray(reinterpret_cast<const char *>(pp), n);
                    text = describe(cmd, pp, n);
                }
            }
            r["text"] = text;
            model()->postEvent(QStringLiteral("result"), r);
        };
        enqueue(p);
    }
}

void MksSession::scan(int from, int to)
{
    from = qBound(1, from, 2047);
    to = qBound(from, to, 2047);
    auto found = QSharedPointer<QList<int>>::create();
    for (int id = from; id <= to; ++id) {
        Pending p;
        p.id = id;
        p.code = mks::code::Version;
        p.priority = 20;
        p.timeoutMs = 40;
        p.done = [this, id, from, to, found](bool ok, const iwp::can::Frame *) {
            if (ok)
                found->append(id);
            QVariantMap e;
            e["done"] = id >= to;
            e["id"] = id;
            e["ok"] = ok;
            e["from"] = from;
            e["to"] = to;
            if (id >= to) {
                QVariantList l;
                for (int f : *found)
                    l.append(f);
                e["found"] = l;
            }
            model()->postEvent(QStringLiteral("scan"), e);
        };
        enqueue(p);
    }
}

void MksSession::readConfig(int id)
{
    // parameters that the endstop / home setup needs; each answer has the format of its "set" command
    static const quint8 codes[] = {mks::code::HomeParams, mks::code::HomeOffset, mks::code::LimitRemap,
                                   mks::code::WorkMode, mks::code::WorkCurrent, mks::code::HoldCurrent};
    for (quint8 c : codes) {
        Pending p;
        p.id = id;
        p.code = mks::code::ReadConfig;
        p.params = {c};
        p.priority = 10;
        p.timeoutMs = 300;
        p.logIt = true;
        p.done = [this, id, c](bool ok, const iwp::can::Frame *frame) {
            QVariantMap r;
            r["id"] = id;
            r["code"] = int(c);
            r["ok"] = false;
            if (ok && frame) {
                quint8 cmd;
                const quint8 *pp;
                int n;
                if (mks::parseResponse(*frame, mks::CrcMode::Sum8, cmd, pp, n)) {
                    // "not supported" is answered as FF FF
                    const bool unsupported = n == 2 && pp[0] == 0xFF && pp[1] == 0xFF;
                    r["ok"] = !unsupported;
                    r["params"] = QByteArray(reinterpret_cast<const char *>(pp), n);
                }
            }
            model()->postEvent(QStringLiteral("config"), r);
        };
        enqueue(p);
    }
}
