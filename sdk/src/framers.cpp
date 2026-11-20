#include "iwp/framers.h"

#include <algorithm>

#include <QtEndian>

namespace iwp {

static constexpr int kMaxLineBuffer = 1 << 20;   // 1 MiB

quint16 crc16CcittFalse(const char *data, int length)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < length; ++i) {
        crc ^= quint16(quint8(data[i])) << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? quint16((crc << 1) ^ 0x1021) : quint16(crc << 1);
    }
    return crc;
}

// ---------------- LineFramer / RawFramer ----------------

QList<QByteArray> LineFramer::feed(const QByteArray &incoming)
{
    QList<QByteArray> frames;
    if (m_terminator.isEmpty())
        return frames;
    m_buffer += incoming;
    int idx;
    while ((idx = m_buffer.indexOf(m_terminator)) >= 0) {
        frames.append(m_buffer.left(idx));
        m_buffer.remove(0, idx + m_terminator.size());
    }
    // garbage without a terminator must not grow without bound
    if (m_buffer.size() > kMaxLineBuffer)
        m_buffer.clear();
    return frames;
}

QList<QByteArray> RawFramer::feed(const QByteArray &incoming)
{
    QList<QByteArray> frames;
    if (!incoming.isEmpty())
        frames.append(incoming);
    return frames;
}

// ---------------- FixedFrameFramer ----------------

bool FixedFrameFramer::crcOk(const char *frame) const
{
    if (m_config.crc == Crc::None || m_config.crcOffset < 0)
        return true;
    const quint16 calc = crc16CcittFalse(frame, m_config.crcOffset);
    const char *field = frame + m_config.crcOffset;
    const quint16 got = m_config.crcLittleEndian ? qFromLittleEndian<quint16>(field)
                                                 : qFromBigEndian<quint16>(field);
    return calc == got;
}

QList<QByteArray> FixedFrameFramer::feed(const QByteArray &incoming)
{
    QList<QByteArray> frames;
    const int len = m_config.length;
    if (len <= 0)
        return frames;

    // Works on an offset and erases the consumed bytes once at the end: with a large chunk
    // (fast streams) removing from the front frame by frame would be quadratic.
    m_buffer += incoming;
    const char *base = m_buffer.constData();
    const int total = int(m_buffer.size());
    int pos = 0;
    while (total - pos >= len) {
        if (!m_config.header.isEmpty()) {
            const int h = int(m_buffer.indexOf(m_config.header, pos));
            if (h < 0) {
                // no header anywhere: keep only a possible partial header at the end
                const int keep = int(m_config.header.size()) - 1;
                const int drop = std::max(0, total - pos - keep);
                m_dropped += quint64(drop);
                pos += drop;
                break;
            }
            if (h > pos) {
                m_dropped += quint64(h - pos);
                pos = h;
                continue;   // re-check that a whole frame is still there
            }
        }
        if (!crcOk(base + pos)) {
            ++m_crcErrors;   // corrupted frame or a header inside data
            ++m_dropped;
            ++pos;
            continue;
        }
        frames.append(QByteArray(base + pos, len));
        pos += len;
        ++m_frames;
    }
    m_buffer.remove(0, pos);
    return frames;
}

QVariantMap FixedFrameFramer::stats() const
{
    return {{"frames", m_frames}, {"crcErrors", m_crcErrors}, {"droppedBytes", m_dropped}};
}

// ---------------- factories ----------------

static QByteArray terminatorFromName(const QString &name)
{
    if (name.isEmpty() || name.compare("NONE", Qt::CaseInsensitive) == 0)
        return {};
    if (name.compare("LF", Qt::CaseInsensitive) == 0)
        return "\n";
    if (name.compare("CR", Qt::CaseInsensitive) == 0)
        return "\r";
    if (name.compare("CRLF", Qt::CaseInsensitive) == 0)
        return "\r\n";
    return name.toUtf8();
}

std::unique_ptr<IFramer> makeFramer(const QVariantMap &framing, QString *error)
{
    auto bad = [error](const QString &msg) -> std::unique_ptr<IFramer> {
        if (error)
            *error = msg;
        return nullptr;
    };

    const QString type = framing.value("type").toString();
    if (type == QLatin1String("raw"))
        return std::make_unique<RawFramer>();

    if (type == QLatin1String("line")) {
        const QByteArray term = terminatorFromName(framing.value("terminator", "LF").toString());
        if (term.isEmpty())
            return bad(QStringLiteral("framing 'line': empty terminator"));
        return std::make_unique<LineFramer>(term);
    }

    if (type == QLatin1String("fixed")) {
        FixedFrameFramer::Config c;
        c.header = QByteArray::fromHex(framing.value("header").toString().toLatin1());
        c.length = framing.value("length").toInt();
        const QString crc = framing.value("crc", "none").toString().toLower();
        if (crc == QLatin1String("ccitt-false"))
            c.crc = FixedFrameFramer::Crc::CcittFalse;
        else if (crc != QLatin1String("none"))
            return bad(QStringLiteral("framing 'fixed': unknown crc '%1'").arg(crc));
        const int crcSize = (c.crc == FixedFrameFramer::Crc::None) ? 0 : 2;
        c.crcOffset = framing.value("crcOffset", c.length - crcSize).toInt();
        c.crcLittleEndian = framing.value("crcEndian", "little").toString().toLower()
                            != QLatin1String("big");
        if (c.length < c.header.size() + crcSize || c.length <= 0)
            return bad(QStringLiteral("framing 'fixed': bad length %1").arg(c.length));
        if (crcSize && (c.crcOffset < c.header.size() || c.crcOffset + 2 > c.length))
            return bad(QStringLiteral("framing 'fixed': bad crcOffset %1").arg(c.crcOffset));
        return std::make_unique<FixedFrameFramer>(c);
    }

    return bad(QStringLiteral("unknown framing type '%1'").arg(type));
}

std::unique_ptr<IFramer> framerFromConfig(const QVariantMap &config, const QString &kind,
                                          QString *error)
{
    if (config.contains("framing"))
        return makeFramer(config.value("framing").toMap(), error);

    const QString fallback = (kind == QLatin1String("scpi")) ? QStringLiteral("LF") : QString();
    const QByteArray term = terminatorFromName(config.value("terminator", fallback).toString());
    if (term.isEmpty())
        return std::make_unique<RawFramer>();
    return std::make_unique<LineFramer>(term);
}

} // namespace iwp
