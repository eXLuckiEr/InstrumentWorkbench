#pragma once
// Monitor of hwmon-like sensors on a Linux device over SSH, without Qt (tested by tools/hwmoncore_test.cpp).
//
// Nothing is installed on the device and nothing is written to its disk. The host starts ONE bash process
// over ssh (bootstrapCommand()) and sends it scripts through stdin, each ended by the line endMarker(). The script
//   1. runs the configured "info" commands once (versions...) and prints their output,
//   2. then reads the sensor files in a loop and prints one line per period, until it is told to STOP,
// using only bash builtins while sampling (no forks), timestamps from $EPOCHREALTIME and a deadline-based
// pause (no drift); the simple mode (Config::precise = false) needs neither bash 5 nor EPOCHREALTIME. When the connection drops the process dies with its terminal.
//
// Lines from the device (tab separated, anything else is ignored):
//   H2                         the bootstrap is ready for a script
//   H                          the OLD bootstrap (length framed receiver, saved in a connection): not usable
//   I <name> <text>            output of an info command (backslash and newline escaped as \\ and \n)
//   E <text>                   a problem (sensor file not found...)
//   L <def> <label>            one per sensor slot, before R: the index of the line of the sensors list it
//                              comes from and, for a name with {} (a mask), the part of the file path
//                              that distinguishes it (hwmon2/temp1_input); empty for a plain sensor
//   R <count> <period_us>      sampling starts
//   S <t_us> <v1> <v2> ...     one sample of all sensors; an empty value = the read failed
//                              t_us is "-" in the simple mode (no $EPOCHREALTIME on the device: the host
//                              stamps the samples when they arrive)
// Lines to the device: STOP
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace hwmon {

constexpr int kMaxSensors = 32;
constexpr int kMinPeriodMs = 20;
constexpr int kMaxPeriodMs = 2000;

struct Sensor {
    std::string name;
    std::string path;       // absolute, a glob is allowed: .../hwmon/hwmon*/curr1_input
    double divisor = 1.0;   // value shown = raw / divisor
    std::string unit;
    bool text = false;      // a state or a flag: the raw text is shown, no graph
    bool hasMin = false, hasMax = false;
    double minV = 0, maxV = 0;   // numeric: the alarm when the value is outside [min, max]
    std::string expect;          // text: the alarm when the value differs
};
// A name with {} is a mask: the path (with * or ?) gives one sensor per matching file.
inline bool isMask(const Sensor &x) { return x.name.find("{}") != std::string::npos; }
// Length of the part of the path before the first component with a wildcard ("/sys/class/hwmon/hwmon*/t" -> 17).
inline size_t staticPrefixLen(const std::string &path)
{
    const size_t w = path.find_first_of("*?[");
    if (w == std::string::npos)
        return 0;
    const size_t slash = path.rfind('/', w);
    return slash == std::string::npos ? 0 : slash + 1;
}
struct Info {
    std::string name;
    std::string command;
};
struct Config {
    int periodMs = 100;
    // true: timestamps from $EPOCHREALTIME (bash 5) and a pause without drift.
    // false: simple mode, the pause is a plain `read -t` (bash 4 is enough), the host stamps the samples.
    bool precise = true;
    std::vector<Sensor> sensors;
    std::vector<Info> infos;
};

// ---- the receiver and the remote command given to ssh (role default "remoteCommand") ----
// The receiver reads lines up to a marker line and evals them, again and again. It reads LINES, not
// "read -N <length>": on a tty (ssh -tt, a UART) bash throws away the input that has already arrived when
// `read -N` starts, so a script sent in one piece was lost (the first connection did nothing, "Apply" then
// glued the pieces together). A stray STOP line (the sampling is not running) is ignored.
inline const char *endMarker() { return "__IWP_END__"; }
inline std::string receiverLoop()
{
    return "while :; do s=; printf -v nl \"\\n\"; while :; do IFS= read -r l || exit 0; [[ $l == STOP ]] && continue; "
           "[[ $l == __IWP_END__ ]] && break; s+=$l$nl; done; eval \"$s\"; done";
}

// stty raw: bytes pass unchanged both ways (no echo, no CR/LF conversion). Prints "H2" when ready ("H" is what the
// old length-framed receiver of a connection saved by an older version printed: the host reports it).
inline std::string bootstrapCommand()
{
    return "stty raw -echo 2>/dev/null; printf 'H2\\n'; exec bash -c '" + receiverLoop() + "'";
}

// ---- text helpers ----
inline std::string trim(const std::string &s)
{
    const char *ws = " \t\r\n";
    const auto a = s.find_first_not_of(ws);
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(ws) - a + 1);
}

inline std::vector<std::string> splitChar(const std::string &s, char sep, size_t maxParts = 0)
{
    std::vector<std::string> out;
    size_t pos = 0;
    while (true) {
        const size_t p = (maxParts && out.size() + 1 >= maxParts) ? std::string::npos : s.find(sep, pos);
        if (p == std::string::npos) {
            out.push_back(s.substr(pos));
            return out;
        }
        out.push_back(s.substr(pos, p - pos));
        pos = p + 1;
    }
}

inline std::string shQuote(const std::string &s)
{
    std::string r = "'";
    for (char c : s) {
        if (c == '\'')
            r += "'\\''";
        else
            r += c;
    }
    return r + "'";
}

inline bool validName(const std::string &n)
{
    if (n.empty() || n.size() > 40)
        return false;
    for (unsigned char c : n)
        if (c < 0x20 || c == '|' || c == '\x7f')
            return false;
    return true;
}

// A path goes into the script unquoted (so that the glob works): allow only harmless characters.
inline bool validPath(const std::string &p)
{
    if (p.size() < 2 || p[0] != '/')
        return false;
    for (unsigned char c : p) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                        || c == '/' || c == '_' || c == '.' || c == '-' || c == '*' || c == '?' || c == '['
                        || c == ']' || c == ':' || c == '@' || c == '+' || c == ',' || c == '=' || c == '~';
        if (!ok)
            return false;
    }
    return true;
}

// ---- the configuration as text (what the user edits) ----
// sensors:  name | path | divisor | unit | options   (all but the path are optional)
//           options: text  min=N  max=N  expect=VALUE;  a name containing {} makes the path a mask
// info:     name | command                      (the command may contain '|')
// '#' starts a comment line, empty lines are ignored.
// The 5th field: words separated by spaces: text | min=N | max=N | expect=VALUE.
inline bool parseOptions(const std::string &field, Sensor &s, int lineNo, std::string &error)
{
    const std::string where = "line " + std::to_string(lineNo) + ": ";
    for (const std::string &tok : splitChar(trim(field), ' ')) {
        if (tok.empty())
            continue;
        const size_t eq = tok.find('=');
        const std::string key = tok.substr(0, eq);
        const std::string val = eq == std::string::npos ? std::string() : tok.substr(eq + 1);
        if (key == "text" && eq == std::string::npos) {
            s.text = true;
        } else if ((key == "min" || key == "max") && !val.empty()) {
            char *end = nullptr;
            const double d = std::strtod(val.c_str(), &end);
            if (*end != '\0') {
                error = where + "a number is expected in " + tok;
                return false;
            }
            (key == "min" ? s.hasMin : s.hasMax) = true;
            (key == "min" ? s.minV : s.maxV) = d;
        } else if (key == "expect" && !val.empty()) {
            s.expect = val;
        } else {
            error = where + "unknown option \"" + tok + "\" (text, min=, max=, expect=)";
            return false;
        }
    }
    if (s.text && (s.hasMin || s.hasMax)) {
        error = where + "min/max are for numbers, use expect= for text";
        return false;
    }
    if (!s.text && !s.expect.empty()) {
        error = where + "expect= needs the text option";
        return false;
    }
    if (s.hasMin && s.hasMax && s.minV > s.maxV) {
        error = where + "min is greater than max";
        return false;
    }
    return true;
}

inline bool parseSensors(const std::string &text, std::vector<Sensor> &out, std::string &error)
{
    out.clear();
    int lineNo = 0;
    for (const std::string &raw : splitChar(text, '\n')) {
        ++lineNo;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#')
            continue;
        const std::vector<std::string> f = splitChar(line, '|');
        Sensor s;
        s.name = trim(f[0]);
        s.path = f.size() > 1 ? trim(f[1]) : std::string();
        if (!validName(s.name)) {
            error = "line " + std::to_string(lineNo) + ": bad sensor name";
            return false;
        }
        if (!validPath(s.path)) {
            error = "line " + std::to_string(lineNo) + ": the path must be absolute, without spaces or special characters";
            return false;
        }
        if (f.size() > 2 && !trim(f[2]).empty()) {
            char *end = nullptr;
            const std::string d = trim(f[2]);
            s.divisor = std::strtod(d.c_str(), &end);
            if (*end != '\0' || !(s.divisor > 0)) {
                error = "line " + std::to_string(lineNo) + ": the divisor must be a number greater than 0";
                return false;
            }
        }
        if (f.size() > 3)
            s.unit = trim(f[3]);
        if (f.size() > 4 && !parseOptions(f[4], s, lineNo, error))
            return false;
        if (f.size() > 5) {
            error = "line " + std::to_string(lineNo) + ": too many fields";
            return false;
        }
        out.push_back(s);
        if (int(out.size()) > kMaxSensors) {
            error = "no more than " + std::to_string(kMaxSensors) + " sensors";
            return false;
        }
    }
    return true;
}

inline bool parseInfos(const std::string &text, std::vector<Info> &out, std::string &error)
{
    out.clear();
    int lineNo = 0;
    for (const std::string &raw : splitChar(text, '\n')) {
        ++lineNo;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#')
            continue;
        const std::vector<std::string> f = splitChar(line, '|', 2);
        Info i;
        i.name = trim(f[0]);
        i.command = f.size() > 1 ? trim(f[1]) : std::string();
        if (!validName(i.name) || i.command.empty()) {
            error = "line " + std::to_string(lineNo) + ": expected \"name | command\"";
            return false;
        }
        out.push_back(i);
    }
    return true;
}

inline std::string defaultSensorsText()
{
    return "# name | file (a * is fine, hwmon numbers change) | divisor | unit | options (text, min=, max=, expect=)\n"
           "curr1 | /sys/bus/i2c/devices/13-0048/hwmon/hwmon*/curr1_input | 1 | mA\n"
           "curr2 | /sys/bus/i2c/devices/13-0048/hwmon/hwmon*/curr2_input | 1 | mA\n"
           "curr3 | /sys/bus/i2c/devices/13-0048/hwmon/hwmon*/curr3_input | 1 | mA\n";
}

inline std::string defaultInfoText()
{
    return "# name | command (run once after connecting, the output is shown at the top)\n"
           "FPGA | cat /sys/kernel/debug/fpga-version/version.0\n"
           "BMC | grep -E '^(PRETTY_NAME|VERSION_ID)=' /etc/os-release\n";
}

// ---- built-in presets (starting points; the user edits them and saves his own as files) ----
struct BuiltinPreset {
    std::string name;
    std::string description;
    int periodMs;
    bool precise;
    std::string sensors;
    std::string info;
};

inline std::vector<BuiltinPreset> builtinPresets()
{
    std::vector<BuiltinPreset> v;
    v.push_back({"OpenBMC: currents (13-0048)", "Three currents of the INA on i2c 13-0048, FPGA and BMC versions", 100, true,
                 defaultSensorsText(), defaultInfoText()});
    v.push_back({"Linux: temperatures and CPU frequency",
                 "thermal zones (milli-degrees) and the frequency of CPU 0; works on most boards", 500, true,
                 "# name | file | divisor | unit\n"
                 "# {} in the name: one tile for every file the * matches; max= turns the tile red above it\n"
                 "zone {} | /sys/class/thermal/thermal_zone*/temp | 1000 | \xC2\xB0" "C | max=85\n"
                 "cpu0 | /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq | 1000 | MHz\n",
                 "# name | command\n"
                 "Kernel | uname -srm\n"
                 "Uptime | uptime\n"});
    v.push_back({"Linux: battery", "Voltage, current and charge of BAT0 (power_supply)", 1000, true,
                 "# name | file | divisor | unit\n"
                 "voltage | /sys/class/power_supply/BAT0/voltage_now | 1000000 | V\n"
                 "current | /sys/class/power_supply/BAT0/current_now | 1000 | mA\n"
                 "charge | /sys/class/power_supply/BAT0/capacity | 1 | % | min=15\n",
                 "# name | command\n"
                 "Status | cat /sys/class/power_supply/BAT0/status\n"});
    v.push_back({"Empty", "Blank list: write your own sensors", 100, true,
                 "# name | file | divisor | unit\n", "# name | command\n"});
    return v;
}

// ---- slots: what the device really reads (after the masks are expanded) ----
struct Slot {
    int def = 0;          // index in Config::sensors
    std::string name;     // shown on the tile
};
// L messages: fields {def, label}. Slots with a bad def are dropped.
inline std::vector<Slot> makeSlots(const Config &cfg, const std::vector<std::vector<std::string>> &lmsgs)
{
    std::vector<Slot> out;
    for (const auto &f : lmsgs) {
        if (f.size() < 2)
            continue;
        char *end = nullptr;
        const long d = std::strtol(f[0].c_str(), &end, 10);
        if (*end != '\0' || d < 0 || d >= long(cfg.sensors.size()) || int(out.size()) >= kMaxSensors)
            continue;
        const Sensor &x = cfg.sensors[size_t(d)];
        Slot sl;
        sl.def = int(d);
        sl.name = x.name;
        if (isMask(x)) {
            const size_t p = sl.name.find("{}");
            sl.name.replace(p, 2, f[1]);
        }
        out.push_back(sl);
    }
    return out;
}
// Without a device: one slot per line of the list (a mask is shown once, under its own name).
inline std::vector<Slot> declaredSlots(const Config &cfg)
{
    std::vector<Slot> out;
    for (size_t i = 0; i < cfg.sensors.size() && int(out.size()) < kMaxSensors; ++i)
        out.push_back({int(i), cfg.sensors[i].name});
    return out;
}
// The alarm: a numeric value outside [min, max], or a text that is not the expected one.
inline bool numberAlarm(const Sensor &x, double v)
{
    return (x.hasMin && v < x.minV) || (x.hasMax && v > x.maxV);
}
inline bool textAlarm(const Sensor &x, const std::string &v)
{
    return !x.expect.empty() && v != x.expect;
}

// ---- the script ----
inline std::string buildScript(const Config &cfg)
{
    std::string s = "shopt -s nullglob\n";
    for (const Info &i : cfg.infos) {
        s += "o=$(eval " + shQuote(i.command) + " 2>&1 </dev/null)\n";
        s += "o=${o//\\\\/\\\\\\\\}\n";           // backslash doubled
        s += "o=${o//$'\\n'/\\\\n}\n";            // newline becomes backslash-n
        s += "printf 'I\\t%s\\t%s\\n' " + shQuote(i.name) + " \"$o\"\n";
    }
    s += "F=()\n";
    const std::string cap = std::to_string(kMaxSensors);
    for (size_t i = 0; i < cfg.sensors.size(); ++i) {
        const Sensor &x = cfg.sensors[i];
        const std::string idx = std::to_string(i);
        if (isMask(x)) {
            s += "set -- " + x.path + "\n";
            s += "if (($#)); then for f; do ((${#F[@]}>=" + cap + ")) && break; F+=(\"$f\"); "
                 "printf 'L\\t" + idx + "\\t%s\\n' \"${f:" + std::to_string(staticPrefixLen(x.path)) + "}\"; done; "
                 "else printf 'E\\t%s\\n' " + shQuote("sensor " + x.name + ": no file matches") + "; fi\n";
        } else {
            s += "set -- " + x.path + "\n";
            s += "if ((${#F[@]}<" + cap + ")); then F+=(\"${1-}\"); printf 'L\\t" + idx + "\\t\\n'; "
                 "[[ -r ${F[-1]} ]] || printf 'E\\t%s\\n' " + shQuote("sensor " + x.name + ": no readable file") + "; fi\n";
        }
    }
    int period = cfg.periodMs;
    if (period < kMinPeriodMs)
        period = kMinPeriodMs;
    if (period > kMaxPeriodMs)
        period = kMaxPeriodMs;
    const long long periodUs = static_cast<long long>(period) * 1000;
    s += "P=" + std::to_string(periodUs) + "\n";
    s += "printf 'R\\t%d\\t%d\\n' ${#F[@]} $P\n";
    if (cfg.precise) {
        s += "n=$((${EPOCHREALTIME/[.,]/}))\n"
             "while :; do\n"
             "  t=${EPOCHREALTIME/[.,]/}\n"
             "  l=S$'\\t'$t\n"
             "  for f in \"${F[@]}\"; do\n"
             "    v=\n"
             "    if [[ -n $f ]]; then { read -r v < \"$f\"; } 2>/dev/null; v=${v//$'\\t'/ }; fi\n"
             "    l+=$'\\t'$v\n"
             "  done\n"
             "  printf '%s\\n' \"$l\"\n"
             "  n=$((n+P)); t=${EPOCHREALTIME/[.,]/}; d=$((n-t))\n"
             "  if ((d<=0)); then n=$t; d=1000; fi\n"
             "  printf -v w '%d.%06d' $((d/1000000)) $((d%1000000))\n"
             "  read -r -t \"$w\" c; k=$?\n"
             "  if ((k==0)); then [[ $c == STOP ]] && break; elif ((k<=128)); then exit; fi\n"
             "done\n";
    } else {
        // Simple: a fixed pause after every sample (the real period is a bit longer by the reading time).
        char w[32];
        std::snprintf(w, sizeof w, "%d.%03d", period / 1000, period % 1000);
        s += "while :; do\n"
             "  l=S$'\\t'-\n"
             "  for f in \"${F[@]}\"; do\n"
             "    v=\n"
             "    if [[ -n $f ]]; then { read -r v < \"$f\"; } 2>/dev/null; v=${v//$'\\t'/ }; fi\n"
             "    l+=$'\\t'$v\n"
             "  done\n"
             "  printf '%s\\n' \"$l\"\n"
             "  read -r -t " + std::string(w) + " c; k=$?\n"
             "  if ((k==0)); then [[ $c == STOP ]] && break; elif ((k<=128)); then exit; fi\n"
             "done\n";
    }
    return s;
}

// What goes to the device: the script and the marker line.
inline std::string frameScript(const std::string &script)
{
    return script + (script.empty() || script.back() != '\n' ? "\n" : "") + endMarker() + "\n";
}

// ---- lines from the device ----
class LineBuffer
{
public:
    // Appends bytes, returns complete lines (without \r\n).
    void feed(const char *data, size_t n, std::vector<std::string> &lines)
    {
        for (size_t i = 0; i < n; ++i) {
            const char c = data[i];
            if (c == '\n') {
                if (!m_cur.empty() && m_cur.back() == '\r')
                    m_cur.pop_back();
                lines.push_back(m_cur);
                m_cur.clear();
            } else if (m_cur.size() < 65536) {
                m_cur += c;
            }
        }
    }
    void clear() { m_cur.clear(); }

private:
    std::string m_cur;
};

struct Message {
    char type = 0;                    // H O I E L R S
    std::vector<std::string> fields;  // after the type
};

inline bool parseMessage(const std::string &line, Message &m)
{
    if (line == "H2" || line == "H") {
        m.type = line == "H2" ? 'H' : 'O';   // 'O': an old receiver is running
        m.fields.clear();
        return true;
    }
    if (line.size() < 2 || line[1] != '\t')
        return false;
    const char t = line[0];
    if (t != 'I' && t != 'E' && t != 'R' && t != 'S' && t != 'L')
        return false;
    m.type = t;
    m.fields = splitChar(line.substr(2), '\t', (t == 'I' || t == 'L') ? 2 : 0);
    return true;
}

inline std::string unescapeInfo(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            out += (s[i] == 'n') ? '\n' : s[i];
        } else {
            out += s[i];
        }
    }
    return out;
}

// S fields: t_us, v1, v2... -> time and values (NaN-free: a failed read is reported in `ok`).
struct Sample {
    long long tUs = 0;
    std::vector<double> value;
    std::vector<bool> ok;
    std::vector<std::string> raw;   // the trimmed text of every value (for the text sensors)
};
inline bool parseSample(const std::vector<std::string> &f, Sample &s)
{
    if (f.empty())
        return false;
    if (f[0] == "-") {
        s.tUs = 0;   // simple mode: the host stamps it
    } else {
        char *end = nullptr;
        s.tUs = std::strtoll(f[0].c_str(), &end, 10);
        if (*end != '\0' || s.tUs <= 0)
            return false;
    }
    s.value.clear();
    s.ok.clear();
    s.raw.clear();
    for (size_t i = 1; i < f.size(); ++i) {
        const std::string t = trim(f[i]);
        s.raw.push_back(t);
        char *e = nullptr;
        const double v = t.empty() ? 0.0 : std::strtod(t.c_str(), &e);
        const bool good = !t.empty() && *e == '\0';
        s.value.push_back(good ? v : 0.0);
        s.ok.push_back(good);
    }
    return true;
}

// Digits after the decimal point for a divisor: 1 -> 0, 1000 -> 3.
inline int decimalsFor(double divisor)
{
    int d = 0;
    double x = 1.0;
    while (x < divisor * 0.999 && d < 6) {
        x *= 10.0;
        ++d;
    }
    return d;
}

} // namespace hwmon
