#pragma once
// Parsing of the answers of the IWP RP2040 base firmware, without Qt (tested by
// tools/rp2040parse_test.cpp).
#include <cstdlib>
#include <string>
#include <vector>

namespace rp2040 {

struct Idn {
    bool ok = false;
    std::string manufacturer, model, serial, firmware;
};

inline std::string trim(const std::string &s)
{
    const char *ws = " \t\r\n";
    const auto a = s.find_first_not_of(ws);
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(ws) - a + 1);
}

// SCPI text results come in quotes: "E6614103E7..." -> E6614103E7...
inline std::string unquote(const std::string &text)
{
    const std::string t = trim(text);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"')
        return t.substr(1, t.size() - 2);
    return t;
}

// "IWP,IWP-RP2040-BASE,E661410403A1B2C3,0.1": manufacturer, model, serial, firmware.
// Commas after the third one belong to the firmware version.
inline Idn parseIdn(const std::string &text)
{
    Idn idn;
    std::string fields[4];
    const std::string t = trim(text);
    size_t pos = 0;
    for (int i = 0; i < 4; ++i) {
        const size_t comma = (i < 3) ? t.find(',', pos) : std::string::npos;
        if (comma == std::string::npos && i < 3)
            return idn;   // fewer than four fields
        fields[i] = trim(t.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
        pos = comma + 1;
    }
    idn.manufacturer = fields[0];
    idn.model = fields[1];
    idn.serial = fields[2];
    idn.firmware = fields[3];
    idn.ok = !idn.model.empty();
    return idn;
}

// "12345,9876" -> two integers (SYSTem:HEAP?: free now, minimum ever free).
inline bool parseTwoNumbers(const std::string &text, long long &a, long long &b)
{
    const std::string t = trim(text);
    const size_t comma = t.find(',');
    if (comma == std::string::npos)
        return false;
    char *end1 = nullptr, *end2 = nullptr;
    const std::string s1 = trim(t.substr(0, comma)), s2 = trim(t.substr(comma + 1));
    if (s1.empty() || s2.empty())
        return false;
    a = std::strtoll(s1.c_str(), &end1, 10);
    b = std::strtoll(s2.c_str(), &end2, 10);
    return *end1 == '\0' && *end2 == '\0';
}

// ---- GPIO:MAP? ----
// One line, thirty quoted strings: "0,UART0_TX,OUT,1,NONE","1,UART0_RX,IN,1,UP",...
struct GpioPin {
    int pin = -1;
    std::string function;   // SPI0_SCK, UART1_TX, I2C1_SDA, PWM3_A, SIO, PIO0, ADC0, CLOCK_GPOUT0, USB_VBUS_EN, XIP, NONE
    bool output = false;
    bool level = false;
    std::string pull;       // NONE | UP | DOWN | BOTH
};

// Splits a SCPI list at the commas that are outside quotes; every element is trimmed and unquoted.
inline std::vector<std::string> splitList(const std::string &text)
{
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (char c : trim(text)) {
        if (c == '"') {
            quoted = !quoted;
            cur += c;
        } else if (c == ',' && !quoted) {
            out.push_back(unquote(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty() || !out.empty())
        out.push_back(unquote(cur));
    return out;
}

// "16,SPI0_RX,IN,0,NONE"
inline bool parseGpioLine(const std::string &line, GpioPin &pin)
{
    const std::vector<std::string> f = splitList(line);
    if (f.size() < 5 || f[0].empty())
        return false;
    char *end = nullptr;
    const long n = std::strtol(f[0].c_str(), &end, 10);
    if (*end != '\0' || n < 0 || n > 63)
        return false;
    pin.pin = static_cast<int>(n);
    pin.function = f[1];
    pin.output = (f[2] == "OUT");
    pin.level = (f[3] == "1");
    pin.pull = f[4];
    return !pin.function.empty();
}

// Whole answer of GPIO:MAP?. Lines that do not parse are skipped.
inline std::vector<GpioPin> parseGpioMap(const std::string &text)
{
    std::vector<GpioPin> pins;
    for (const std::string &element : splitList(text)) {
        GpioPin p;
        if (parseGpioLine(element, p))
            pins.push_back(p);
    }
    return pins;
}

} // namespace rp2040
