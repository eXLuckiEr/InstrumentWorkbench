#pragma once
#include <QtEndian>
#include <cstring>

// Helpers for binary protocols. `p` points into a frame.
namespace iwp {

inline quint16 readU16LE(const char *p) { return qFromLittleEndian<quint16>(p); }
inline quint32 readU32LE(const char *p) { return qFromLittleEndian<quint32>(p); }
inline qint32 readI32LE(const char *p) { return qFromLittleEndian<qint32>(p); }
inline float readF32LE(const char *p)
{
    const quint32 u = qFromLittleEndian<quint32>(p);
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}

inline quint16 readU16BE(const char *p) { return qFromBigEndian<quint16>(p); }
inline quint32 readU32BE(const char *p) { return qFromBigEndian<quint32>(p); }
inline float readF32BE(const char *p)
{
    const quint32 u = qFromBigEndian<quint32>(p);
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}

inline void writeU16LE(char *p, quint16 v) { qToLittleEndian<quint16>(v, p); }
inline void writeF32LE(char *p, float f)
{
    quint32 u;
    std::memcpy(&u, &f, sizeof u);
    qToLittleEndian<quint32>(u, p);
}

} // namespace iwp
