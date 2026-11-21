#pragma once
// Text of a frame for the tables (Qt strings built from the Qt-free core).
#include <QString>

#include "iwp/cancore.h"

namespace canfmt {

inline QString idText(const iwp::can::Frame &f)
{
    return QString::fromStdString(iwp::can::idText(f.id, f.extended()));
}

inline QString dataText(const iwp::can::Frame &f)
{
    return f.rtr() ? QString() : QString::fromStdString(iwp::can::toHex(f.data, f.len, true));
}

inline QString asciiText(const iwp::can::Frame &f)
{
    if (f.rtr())
        return {};
    QString s;
    s.reserve(f.len);
    for (int i = 0; i < f.len; ++i)
        s.append(f.data[i] >= 32 && f.data[i] < 127 ? QChar(QLatin1Char(char(f.data[i]))) : QChar(QLatin1Char('.')));
    return s;
}

inline QString flagsText(const iwp::can::Frame &f)
{
    QString s = f.extended() ? QStringLiteral("EXT") : QStringLiteral("STD");
    if (f.fd())
        s += QStringLiteral(" FD");
    if (f.has(iwp::can::Brs))
        s += QStringLiteral(" BRS");
    if (f.has(iwp::can::Esi))
        s += QStringLiteral(" ESI");
    if (f.rtr())
        s += QStringLiteral(" RTR");
    if (f.has(iwp::can::Error))
        s += QStringLiteral(" ERR");
    return s;
}

inline QString dlcText(const iwp::can::Frame &f)
{
    if (f.fd()) {
        const int dlc = iwp::can::lenToDlc(f.len, true);
        return f.len > 8 ? QStringLiteral("%1 [%2]").arg(dlc).arg(f.len) : QString::number(f.len);
    }
    return QString::number(f.len);
}

inline QString seconds(quint64 us) { return QString::number(double(us) / 1e6, 'f', 6); }

} // namespace canfmt
