#pragma once
#include <memory>

#include <QString>

#include "iwp/iframer.h"

namespace iwp {

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xorout.
IWP_SDK_EXPORT quint16 crc16CcittFalse(const char *data, int length);

// Text protocols: frame = line ended by a terminator (SCPI...).
// encode() appends the terminator.
class IWP_SDK_EXPORT LineFramer : public IFramer
{
public:
    explicit LineFramer(const QByteArray &terminator) : m_terminator(terminator) {}
    QByteArray encode(const QByteArray &payload) override { return payload + m_terminator; }
    QList<QByteArray> feed(const QByteArray &incoming) override;
    void reset() override { m_buffer.clear(); }

private:
    QByteArray m_terminator;
    QByteArray m_buffer;
};

// No framing: every received chunk is a frame and never an answer.
// NOTE: chunk boundaries are arbitrary (OS buffers, drivers split data), so
// this is only for consoles and raw monitoring, never for a real protocol.
class IWP_SDK_EXPORT RawFramer : public IFramer
{
public:
    QList<QByteArray> feed(const QByteArray &incoming) override;
    bool matches(const QByteArray &) const override { return false; }
};

// Continuous binary stream of equally sized frames:
//   [header][...][CRC16] ; length fixed.
// Finds frames by header, validates the CRC, and resynchronises after
// garbage or lost bytes (a header found inside data fails the CRC check and
// is skipped). Frames are push data: matches() is false.
class IWP_SDK_EXPORT FixedFrameFramer : public IFramer
{
public:
    enum class Crc { None, CcittFalse };
    struct Config {
        QByteArray header;            // bytes in stream order, may be empty
        int length = 0;               // whole frame including header and CRC
        Crc crc = Crc::None;
        int crcOffset = -1;           // CRC position; covers bytes [0, crcOffset)
        bool crcLittleEndian = true;  // byte order of the CRC field
    };

    explicit FixedFrameFramer(const Config &config) : m_config(config) {}
    QList<QByteArray> feed(const QByteArray &incoming) override;
    bool matches(const QByteArray &) const override { return false; }
    void reset() override { m_buffer.clear(); }
    QVariantMap stats() const override;

private:
    bool crcOk(const char *frame) const;

    Config m_config;
    QByteArray m_buffer;
    quint64 m_frames = 0;
    quint64 m_crcErrors = 0;
    quint64 m_dropped = 0;
};

// Builds a framer from a config object:
//   {"type":"line","terminator":"LF"}
//   {"type":"raw"}
//   {"type":"fixed","header":"AA55","length":32,
//    "crc":"ccitt-false","crcOffset":30,"crcEndian":"little"}
// Returns nullptr and sets *error on a bad config.
IWP_SDK_EXPORT std::unique_ptr<IFramer> makeFramer(const QVariantMap &framing,
                                                   QString *error = nullptr);

// Endpoint config -> framer: "framing" object if present, else the
// "terminator" key ("LF"/"CR"/"CRLF"/text, ""=raw; default LF for kind
// "scpi", raw otherwise).
IWP_SDK_EXPORT std::unique_ptr<IFramer> framerFromConfig(const QVariantMap &config,
                                                         const QString &kind,
                                                         QString *error = nullptr);

} // namespace iwp
