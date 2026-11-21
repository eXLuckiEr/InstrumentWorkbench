#pragma once
// SLCAN (Lawicel) text protocol of USB-CAN adapters: frames <-> lines. No Qt, testable alone.
//
//   t1238AABBCCDDEEFF0011\r    standard frame: 't' id(3 hex) dlc(1 hex) data
//   T123456788AABB...\r        extended frame: 'T' id(8 hex) dlc data
//   r1234\r / R123456784\r     remote frame
//   d / D                      CAN FD frame (no BRS), standard / extended: id, dlc char (0..F), data
//   b / B                      CAN FD frame with BRS
// Bit rates: "S6" = 500 kbit/s, "S8" = 1 Mbit/s; FD data rate "Y2" = 2 Mbit/s (CANable 2 firmware).
#include <cstdint>
#include <string>

#include "iwp/cancore.h"

namespace iwp {
namespace can {
namespace slcan {

// the text for a frame to send, with the closing '\r'; empty if the frame cannot be sent
inline std::string encode(const Frame &f)
{
    static const char digits[] = "0123456789ABCDEF";
    std::string s;
    const bool ext = f.extended();
    char kind;
    if (f.fd())
        kind = f.has(Brs) ? (ext ? 'B' : 'b') : (ext ? 'D' : 'd');
    else if (f.rtr())
        kind = ext ? 'R' : 'r';
    else
        kind = ext ? 'T' : 't';
    s.push_back(kind);
    s += idText(f.id, ext);
    int dlc = f.fd() ? lenToDlc(f.len, true) : f.len;
    if (dlc < 0 || (!f.fd() && f.len > 8))
        return {};
    s.push_back(digits[dlc]);
    if (!f.rtr())
        s += toHex(f.data, f.len, false);
    s.push_back('\r');
    return s;
}

// A received line (without '\r'). False if it is not a frame (an answer to a command, noise).
// Trailing characters after the data (a timestamp, if the adapter sends one) are ignored.
inline bool decode(const std::string &line, Frame &out)
{
    if (line.empty())
        return false;
    const char kind = line[0];
    bool ext, fd = false, brs = false, rtr = false;
    switch (kind) {
    case 't': ext = false; break;
    case 'T': ext = true; break;
    case 'r': ext = false; rtr = true; break;
    case 'R': ext = true; rtr = true; break;
    case 'd': ext = false; fd = true; break;
    case 'D': ext = true; fd = true; break;
    case 'b': ext = false; fd = true; brs = true; break;
    case 'B': ext = true; fd = true; brs = true; break;
    default: return false;
    }
    const std::size_t idDigits = ext ? 8 : 3;
    if (line.size() < 1 + idDigits + 1)
        return false;
    std::uint32_t id = 0;
    for (std::size_t i = 0; i < idDigits; ++i) {
        const int h = hexValue(line[1 + i]);
        if (h < 0)
            return false;
        id = id * 16 + std::uint32_t(h);
    }
    if (id > maxId(ext))
        return false;
    const int dlc = hexValue(line[1 + idDigits]);
    if (dlc < 0)
        return false;
    Frame f;
    f.id = id;
    f.flags = std::uint8_t((ext ? Extended : 0) | (fd ? Fd : 0) | (brs ? Brs : 0) | (rtr ? Rtr : 0));
    const int len = fd ? dlcToLen(dlc) : dlc;
    if (len < 0 || (!fd && len > 8))
        return false;
    f.len = std::uint8_t(len);
    if (!rtr) {
        const std::size_t start = 2 + idDigits;
        if (line.size() < start + std::size_t(len) * 2)
            return false;
        for (int i = 0; i < len; ++i) {
            const int hi = hexValue(line[start + std::size_t(i) * 2]);
            const int lo = hexValue(line[start + std::size_t(i) * 2 + 1]);
            if (hi < 0 || lo < 0)
                return false;
            f.data[i] = std::uint8_t(hi * 16 + lo);
        }
    }
    out = f;
    return true;
}

// "S<digit>" for the nominal bit rate; '\0' if the adapter has no such preset
inline char nominalCode(int bitrate)
{
    switch (bitrate) {
    case 10000: return '0';
    case 20000: return '1';
    case 50000: return '2';
    case 100000: return '3';
    case 125000: return '4';
    case 250000: return '5';
    case 500000: return '6';
    case 1000000: return '8';
    default: return '\0';
    }
}

// "Y<digit>" for the FD data bit rate; '\0' if unknown
inline char dataCode(int bitrate)
{
    switch (bitrate) {
    case 1000000: return '1';
    case 2000000: return '2';
    case 4000000: return '4';
    case 5000000: return '5';
    default: return '\0';
    }
}

} // namespace slcan
} // namespace can
} // namespace iwp
