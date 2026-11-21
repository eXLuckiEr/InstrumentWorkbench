#pragma once
// CAN / CAN FD core without Qt: the frame, DLC arithmetic, hex and candump-style text, the ID filter.
// Header only, so it can be tested on its own (tools/cancore_test.cpp).
//
// Text format of one frame (the one of can-utils: candump/cansend):
//   123#DEADBEEF        standard frame (up to 3 hex digits of ID)
//   12345678#DEAD       extended frame (4..8 hex digits of ID)
//   123#R               remote frame ("123#R4" = remote frame with DLC 4)
//   123##1DEADBEEF      CAN FD frame; the digit after "##" are flags (bit 0 = BRS, bit 1 = ESI)
// Spaces between bytes are allowed: "123#DE AD BE EF".
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace iwp {
namespace can {

enum Flag : std::uint8_t {
    Extended = 1,   // 29-bit ID
    Fd = 2,         // CAN FD frame
    Brs = 4,        // FD: data phase at the higher bit rate
    Esi = 8,        // FD: error state indicator
    Rtr = 16,       // remote frame (classic CAN only)
    Error = 32,     // error frame (id/data are adapter specific)
    Tx = 64         // we sent it (not received from the bus)
};

constexpr int kMaxData = 64;

struct Frame {
    std::uint64_t tsUs = 0;      // microseconds since the bus was opened
    std::uint32_t id = 0;
    std::uint8_t len = 0;        // number of data bytes (for a remote frame: its DLC)
    std::uint8_t flags = 0;
    std::uint8_t data[kMaxData] = {};

    bool has(Flag f) const { return (flags & f) != 0; }
    bool extended() const { return has(Extended); }
    bool fd() const { return has(Fd); }
    bool rtr() const { return has(Rtr); }
    bool tx() const { return has(Tx); }
    // bytes that are really on the bus (a remote frame carries none)
    int payload() const { return rtr() ? 0 : len; }
};

// ---- DLC <-> length ----
inline int dlcToLen(int dlc)
{
    static const int table[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
    return (dlc < 0 || dlc > 15) ? -1 : table[dlc];
}

// -1 if `len` is not a length that exists (FD: 0..8, 12, 16, 20, 24, 32, 48, 64; classic: 0..8)
inline int lenToDlc(int len, bool fd)
{
    if (len < 0)
        return -1;
    if (len <= 8)
        return len;
    if (!fd)
        return -1;
    for (int dlc = 9; dlc <= 15; ++dlc)
        if (dlcToLen(dlc) == len)
            return dlc;
    return -1;
}

inline std::uint32_t maxId(bool extended) { return extended ? 0x1FFFFFFFu : 0x7FFu; }

// How long the frame occupies the bus, in seconds. An estimate: no stuff bits, the FD CRC length
// is taken for the data size. Good enough for the "bus load" figure.
inline double busTimeSeconds(const Frame &f, double nominalBitrate, double dataBitrate)
{
    if (nominalBitrate <= 0)
        return 0;
    const int n = f.payload();
    if (!f.fd())
        return ((f.extended() ? 67.0 : 47.0) + 8.0 * n) / nominalBitrate;
    const double arbitration = f.extended() ? 48.0 : 28.0;           // up to BRS
    const double dataPhase = 8.0 * n + (n <= 16 ? 17.0 : 21.0) + 9.0; // control tail + data + CRC
    const double tail = 12.0;                                        // ACK, EOF, IFS
    const double dr = (f.has(Brs) && dataBitrate > 0) ? dataBitrate : nominalBitrate;
    return (arbitration + tail) / nominalBitrate + dataPhase / dr;
}

// ---- hex ----
inline int hexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline std::string toHex(const std::uint8_t *p, int n, bool spaced)
{
    static const char digits[] = "0123456789ABCDEF";
    std::string s;
    s.reserve(std::size_t(n) * 3);
    for (int i = 0; i < n; ++i) {
        if (spaced && i)
            s.push_back(' ');
        s.push_back(digits[p[i] >> 4]);
        s.push_back(digits[p[i] & 15]);
    }
    return s;
}

inline std::string idText(std::uint32_t id, bool extended)
{
    char b[16];
    std::snprintf(b, sizeof b, extended ? "%08X" : "%03X", static_cast<unsigned>(id));
    return b;
}

// Bytes from text: "DE AD BE EF", "DEADBEEF", "0xDE,0xAD", "1 2 3" (a lone digit is one byte).
// A word of 3 or more digits must have an even number of them. Returns false and a message on error.
inline bool parseHexBytes(const std::string &text, std::uint8_t *out, int maxBytes, int &count,
                          std::string &error)
{
    count = 0;
    std::size_t i = 0;
    const std::size_t n = text.size();
    auto isSep = [](char c) { return c == ' ' || c == '\t' || c == ',' || c == '_' || c == '.' || c == ':'; };
    while (i < n) {
        while (i < n && isSep(text[i]))
            ++i;
        if (i >= n)
            break;
        std::size_t j = i;
        while (j < n && !isSep(text[j]))
            ++j;
        std::string word = text.substr(i, j - i);
        i = j;
        if (word.size() >= 2 && word[0] == '0' && (word[1] == 'x' || word[1] == 'X'))
            word = word.substr(2);
        if (word.empty()) {
            error = "empty byte after 0x";
            return false;
        }
        for (char c : word)
            if (hexValue(c) < 0) {
                error = std::string("not a hex digit: '") + c + "'";
                return false;
            }
        if (word.size() > 2 && (word.size() % 2) != 0) {
            error = "odd number of hex digits in \"" + word + "\"";
            return false;
        }
        for (std::size_t k = 0; k < word.size(); k += (word.size() == 1 ? 1 : 2)) {
            const int v = word.size() == 1 ? hexValue(word[0])
                                           : hexValue(word[k]) * 16 + hexValue(word[k + 1]);
            if (count >= maxBytes) {
                error = "too many data bytes (maximum " + std::to_string(maxBytes) + ")";
                return false;
            }
            out[count++] = static_cast<std::uint8_t>(v);
        }
    }
    return true;
}

namespace detail {

inline std::string trim(const std::string &s)
{
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
        ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
        --b;
    return s.substr(a, b - a);
}

inline bool parseHexNumber(const std::string &s, std::uint32_t &out)
{
    std::string t = s;
    if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'))
        t = t.substr(2);
    if (t.empty() || t.size() > 8)
        return false;
    std::uint64_t v = 0;
    for (char c : t) {
        const int h = hexValue(c);
        if (h < 0)
            return false;
        v = v * 16 + std::uint64_t(h);
    }
    out = static_cast<std::uint32_t>(v);
    return true;
}

} // namespace detail

// One frame from text (see the top of this file). `error` explains a failure.
inline bool parseFrame(const std::string &text, Frame &out, std::string &error)
{
    Frame f;
    const std::string s = detail::trim(text);
    const std::size_t hash = s.find('#');
    if (hash == std::string::npos || hash == 0) {
        error = "expected ID#DATA";
        return false;
    }
    const std::string idText = detail::trim(s.substr(0, hash));
    std::uint32_t id = 0;
    if (!detail::parseHexNumber(idText, id)) {
        error = "bad ID \"" + idText + "\"";
        return false;
    }
    const bool hasPrefix = idText.size() > 2 && idText[0] == '0' && (idText[1] == 'x' || idText[1] == 'X');
    const std::size_t digits = hasPrefix ? idText.size() - 2 : idText.size();
    const bool ext = digits > 3;
    if (id > maxId(ext)) {
        error = ext ? "ID is larger than 1FFFFFFF" : "standard ID is larger than 7FF (use 8 digits for an extended ID)";
        return false;
    }
    f.id = id;
    if (ext)
        f.flags |= Extended;

    std::string rest = s.substr(hash + 1);
    if (!rest.empty() && rest[0] == '#') {   // CAN FD: ID##<flags><data>
        f.flags |= Fd;
        rest = detail::trim(rest.substr(1));
        if (rest.empty()) {
            error = "CAN FD frame needs the flags digit after ##";
            return false;
        }
        const int fl = hexValue(rest[0]);
        if (fl < 0 || fl > 3) {
            error = "FD flags must be 0..3 (1 = BRS, 2 = ESI)";
            return false;
        }
        if (fl & 1) f.flags |= Brs;
        if (fl & 2) f.flags |= Esi;
        rest = rest.substr(1);
    } else {
        rest = detail::trim(rest);
        if (!rest.empty() && (rest[0] == 'R' || rest[0] == 'r')) {   // remote frame
            f.flags |= Rtr;
            const std::string dlc = detail::trim(rest.substr(1));
            int len = 0;
            if (!dlc.empty()) {
                if (dlc.size() != 1 || hexValue(dlc[0]) < 0 || hexValue(dlc[0]) > 8) {
                    error = "remote frame DLC must be 0..8";
                    return false;
                }
                len = hexValue(dlc[0]);
            }
            f.len = static_cast<std::uint8_t>(len);
            out = f;
            return true;
        }
    }

    int count = 0;
    if (!parseHexBytes(rest, f.data, f.fd() ? kMaxData : 8, count, error))
        return false;
    if (lenToDlc(count, f.fd()) < 0) {
        error = f.fd() ? "FD length must be 0..8, 12, 16, 20, 24, 32, 48 or 64 bytes (got "
                             + std::to_string(count) + ")"
                       : "classic CAN carries at most 8 bytes (got " + std::to_string(count) + ")";
        return false;
    }
    f.len = static_cast<std::uint8_t>(count);
    out = f;
    return true;
}

inline std::string formatFrame(const Frame &f)
{
    std::string s = idText(f.id, f.extended()) + "#";
    if (f.fd()) {
        s += '#';
        s += char('0' + ((f.has(Brs) ? 1 : 0) | (f.has(Esi) ? 2 : 0)));
        s += toHex(f.data, f.len, false);
    } else if (f.rtr()) {
        s += 'R';
        if (f.len)
            s += char('0' + f.len);
    } else {
        s += toHex(f.data, f.len, false);
    }
    return s;
}

// candump -l style line: "(12.345678) can0 123#DEADBEEF"
inline std::string formatLogLine(const Frame &f, const std::string &iface)
{
    char b[48];
    std::snprintf(b, sizeof b, "(%llu.%06llu) ", static_cast<unsigned long long>(f.tsUs / 1000000),
                  static_cast<unsigned long long>(f.tsUs % 1000000));
    return std::string(b) + iface + " " + formatFrame(f);
}

// ---- sequence of frames with pauses ----
struct SequenceItem {
    bool isDelay = false;
    Frame frame;
    int delayMs = 0;
};

// One item per line: a frame (see above), or "wait 100" / "sleep 100" / "delay 100" (milliseconds).
// Empty lines and lines starting with '#' or "//" are skipped. On error `errorLine` is the
// 1-based line number.
inline bool parseSequence(const std::string &text, std::vector<SequenceItem> &out, std::string &error,
                          int &errorLine)
{
    out.clear();
    errorLine = 0;
    std::size_t pos = 0;
    int lineNo = 0;
    while (pos <= text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        const std::string line = detail::trim(text.substr(pos, end - pos));
        pos = end + 1;
        ++lineNo;
        if (line.empty() || line[0] == '#' || line.compare(0, 2, "//") == 0)
            continue;
        SequenceItem item;
        std::size_t sp = line.find_first_of(" \t");
        std::string word = line.substr(0, sp);
        for (char &c : word)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (word == "wait" || word == "sleep" || word == "delay") {
            std::string num = sp == std::string::npos ? std::string() : detail::trim(line.substr(sp));
            char *endp = nullptr;
            const long ms = std::strtol(num.c_str(), &endp, 10);
            if (num.empty() || *endp != '\0' || ms < 0 || ms > 3600000) {
                error = "bad pause (milliseconds expected)";
                errorLine = lineNo;
                return false;
            }
            item.isDelay = true;
            item.delayMs = static_cast<int>(ms);
        } else if (!parseFrame(line, item.frame, error)) {
            errorLine = lineNo;
            return false;
        }
        out.push_back(item);
    }
    return true;
}

// ---- ID filter ----
// "123, 100-1FF, 7E0:7F0, !7FF": exact ID, range, ID with mask (id & mask == value & mask),
// a leading '!' excludes. With no include rule everything passes except the excluded.
class IdFilter
{
public:
    bool parse(const std::string &text, std::string &error)
    {
        std::vector<Rule> rules;
        std::size_t pos = 0;
        while (pos <= text.size()) {
            std::size_t end = text.find_first_of(",;", pos);
            if (end == std::string::npos)
                end = text.size();
            std::string item = detail::trim(text.substr(pos, end - pos));
            pos = end + 1;
            if (item.empty())
                continue;
            Rule r;
            if (item[0] == '!') {
                r.exclude = true;
                item = detail::trim(item.substr(1));
            }
            const std::size_t dash = item.find('-');
            const std::size_t colon = item.find(':');
            std::uint32_t a = 0, b = 0;
            if (dash != std::string::npos) {
                if (!detail::parseHexNumber(detail::trim(item.substr(0, dash)), a)
                    || !detail::parseHexNumber(detail::trim(item.substr(dash + 1)), b) || a > b) {
                    error = "bad range \"" + item + "\"";
                    return false;
                }
                r.lo = a; r.hi = b;
            } else if (colon != std::string::npos) {
                if (!detail::parseHexNumber(detail::trim(item.substr(0, colon)), a)
                    || !detail::parseHexNumber(detail::trim(item.substr(colon + 1)), b)) {
                    error = "bad id:mask \"" + item + "\"";
                    return false;
                }
                r.useMask = true; r.lo = a & b; r.mask = b;
            } else {
                if (!detail::parseHexNumber(item, a)) {
                    error = "bad ID \"" + item + "\"";
                    return false;
                }
                r.lo = r.hi = a;
            }
            rules.push_back(r);
        }
        m_rules = rules;
        return true;
    }

    bool empty() const { return m_rules.empty(); }

    bool accepts(std::uint32_t id) const
    {
        bool anyInclude = false, included = false;
        for (const Rule &r : m_rules) {
            const bool hit = r.useMask ? ((id & r.mask) == r.lo) : (id >= r.lo && id <= r.hi);
            if (r.exclude) {
                if (hit)
                    return false;
            } else {
                anyInclude = true;
                included = included || hit;
            }
        }
        return !anyInclude || included;
    }

private:
    struct Rule {
        std::uint32_t lo = 0, hi = 0, mask = 0;
        bool useMask = false;
        bool exclude = false;
    };
    std::vector<Rule> m_rules;
};

} // namespace can
} // namespace iwp
