#pragma once
// Monitor of hwmon-like sensors of a Linux device over the SSH command channel, without Qt (tested by
// tools/hwmoncore_test.cpp).
//
// Nothing is installed on the device and nothing is written to its disk. Two small POSIX sh scripts are run
// through the ordinary "ssh" endpoint (one request = one `sh` evaluation, plain pipes, no terminal), so they
// work on busybox too and the banner or the prompt of the device never gets into the data:
//   * the DISCOVERY script, once after connecting (and after "Apply"): runs the info commands (versions...),
//     expands the masks and reports the files that will be read,
//   * the POLL script, every period: reads those files with the `read` builtin (no forks) and prints one line.
//
// Lines from the device (anything else is ignored):
//   i <index> <text>            one line of the output of the info command <index> (tab separated)
//   L <def> <label> <path>      one per sensor slot: the line of the sensors list it comes from, for a name
//                               with {} (a mask) the part of the path that distinguishes it, the file
//   E <text>                    a problem (sensor file not found...)
//   S <v1> <v2> ...             one sample of all slots; an empty value = the read failed
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
    std::vector<Sensor> sensors;
    std::vector<Info> infos;
};

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
    std::string sensors;
    std::string info;
};

inline std::vector<BuiltinPreset> builtinPresets()
{
    std::vector<BuiltinPreset> v;
    v.push_back({"OpenBMC: currents (13-0048)", "Three currents of the INA on i2c 13-0048, FPGA and BMC versions", 100,
                 defaultSensorsText(), defaultInfoText()});
    v.push_back({"Linux: temperatures and CPU frequency",
                 "thermal zones (milli-degrees) and the frequency of CPU 0; works on most boards", 500,
                 "# name | file | divisor | unit\n"
                 "# {} in the name: one tile for every file the * matches; max= turns the tile red above it\n"
                 "zone {} | /sys/class/thermal/thermal_zone*/temp | 1000 | \xC2\xB0" "C | max=85\n"
                 "cpu0 | /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq | 1000 | MHz\n",
                 "# name | command\n"
                 "Kernel | uname -srm\n"
                 "Uptime | uptime\n"});
    v.push_back({"Linux: battery", "Voltage, current and charge of BAT0 (power_supply)", 1000,
                 "# name | file | divisor | unit\n"
                 "voltage | /sys/class/power_supply/BAT0/voltage_now | 1000000 | V\n"
                 "current | /sys/class/power_supply/BAT0/current_now | 1000 | mA\n"
                 "charge | /sys/class/power_supply/BAT0/capacity | 1 | % | min=15\n",
                 "# name | command\n"
                 "Status | cat /sys/class/power_supply/BAT0/status\n"});
    v.push_back({"Empty", "Blank list: write your own sensors", 100,
                 "# name | file | divisor | unit\n", "# name | command\n"});
    return v;
}

// ---- slots: what the device really reads (after the masks are expanded) ----
struct Slot {
    int def = 0;          // index in Config::sensors
    std::string name;     // shown on the tile
    std::string path;     // the file to read (empty before the device has reported it)
};
inline bool safePath(const std::string &p)
{
    return p.size() >= 2 && p[0] == '/' && p.find_first_of(" \t'\"\\$`;&|<>(){}\n*?[") == std::string::npos;
}
// L messages: fields {def, label, path}. Slots with a bad def are dropped.
inline std::vector<Slot> makeSlots(const Config &cfg, const std::vector<std::vector<std::string>> &lmsgs)
{
    std::vector<Slot> out;
    for (const auto &f : lmsgs) {
        if (f.size() < 3)
            continue;
        char *end = nullptr;
        const long d = std::strtol(f[0].c_str(), &end, 10);
        if (*end != '\0' || d < 0 || d >= long(cfg.sensors.size()) || int(out.size()) >= kMaxSensors)
            continue;
        const Sensor &x = cfg.sensors[size_t(d)];
        Slot sl;
        sl.def = int(d);
        sl.name = x.name;
        sl.path = safePath(f[2]) ? f[2] : std::string();   // an unmatched glob: the slot stays, its value is empty
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
        out.push_back({int(i), cfg.sensors[i].name, std::string()});
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

// ---- the scripts (POSIX sh) ----

// Once after connecting: versions, the files behind the masks.
inline std::string buildDiscoveryScript(const Config &cfg)
{
    std::string s = "c=0\n";
    for (size_t i = 0; i < cfg.infos.size(); ++i) {
        const std::string idx = std::to_string(i);
        s += "o=$(eval " + shQuote(cfg.infos[i].command) + " 2>&1 </dev/null)\n";
        s += "printf '%s\\n' \"$o\" | while IFS= read -r l; do printf 'i\\t" + idx + "\\t%s\\n' \"$l\"; done\n";
    }
    const std::string cap = std::to_string(kMaxSensors);
    for (size_t i = 0; i < cfg.sensors.size(); ++i) {
        const Sensor &x = cfg.sensors[i];
        const std::string idx = std::to_string(i);
        if (isMask(x)) {
            const std::string prefix = x.path.substr(0, staticPrefixLen(x.path));
            s += "m=0; for f in " + x.path + "; do [ -e \"$f\" ] || continue; [ \"$c\" -lt " + cap + " ] || break; "
                 "c=$((c+1)); m=1; printf 'L\\t" + idx + "\\t%s\\t%s\\n' \"${f#" + prefix + "}\" \"$f\"; done\n";
            s += "[ \"$m\" = 1 ] || printf 'E\\t%s\\n' " + shQuote("sensor " + x.name + ": no file matches") + "\n";
        } else {
            s += "set -- " + x.path + "\n";   // a glob gives its first match, no match leaves the pattern
            s += "if [ \"$c\" -lt " + cap + " ]; then c=$((c+1)); printf 'L\\t" + idx + "\\t\\t%s\\n' \"$1\"; "
                 "[ -r \"$1\" ] || printf 'E\\t%s\\n' " + shQuote("sensor " + x.name + ": no readable file") + "; fi\n";
        }
    }
    s += "true\n";
    return s;
}

// Every period: one line with the value of every slot, read with a builtin (no forks).
inline std::string buildPollScript(const std::vector<Slot> &slotList)
{
    std::string files;
    for (const Slot &sl : slotList)
        files += " " + shQuote(safePath(sl.path) ? sl.path : std::string());
    // a tab inside a value would shift the columns: replaced (a fork, only when there is one)
    return "t='\t'; printf S; for f in" + files + "; do v=; { read -r v < \"$f\"; } 2>/dev/null; "
           "case $v in *\"$t\"*) v=$(printf %s \"$v\" | tr \"$t\" ' ');; esac; printf '\\t%s' \"$v\"; done; echo\n";
}

// ---- what comes back ----
struct Discovery {
    std::vector<std::string> info;                      // the text of every info command, in the order of Config::infos
    std::vector<std::vector<std::string>> slotMsgs;     // L: {def, label, path}
    std::vector<std::string> errors;                    // E
    std::vector<std::string> stray;                     // anything else (shell messages)
};

inline std::vector<std::string> splitLines(const std::string &text)
{
    std::vector<std::string> lines = splitChar(text, '\n');
    for (std::string &l : lines)
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
    return lines;
}

inline Discovery parseDiscovery(const Config &cfg, const std::string &text)
{
    Discovery d;
    d.info.assign(cfg.infos.size(), std::string());
    std::vector<bool> any(cfg.infos.size(), false);
    for (const std::string &l : splitLines(text)) {
        if (l.empty())
            continue;
        if (l.size() > 2 && l[1] == '\t' && (l[0] == 'i' || l[0] == 'L' || l[0] == 'E')) {
            if (l[0] == 'i') {
                const std::vector<std::string> f = splitChar(l.substr(2), '\t', 2);
                const long idx = std::strtol(f[0].c_str(), nullptr, 10);
                if (f.size() == 2 && idx >= 0 && idx < long(d.info.size())) {
                    std::string &t = d.info[size_t(idx)];
                    t += (any[size_t(idx)] ? "\n" : "") + f[1];
                    any[size_t(idx)] = true;
                    continue;
                }
            } else if (l[0] == 'L') {
                d.slotMsgs.push_back(splitChar(l.substr(2), '\t', 3));
                continue;
            } else {
                d.errors.push_back(l.substr(2));
                continue;
            }
        }
        d.stray.push_back(l);
    }
    return d;
}

// One sample: the values of the last "S" line of the answer (anything else in it is returned in `stray`).
struct Sample {
    std::vector<double> value;
    std::vector<bool> ok;
    std::vector<std::string> raw;   // the trimmed text of every value (for the text sensors)
};
inline bool parseSample(const std::string &text, Sample &s, std::vector<std::string> *stray = nullptr)
{
    bool found = false;
    for (const std::string &l : splitLines(text)) {
        if (l == "S" || (l.size() > 1 && l[0] == 'S' && l[1] == '\t')) {
            s.value.clear();
            s.ok.clear();
            s.raw.clear();
            if (l.size() > 2) {
                for (const std::string &f : splitChar(l.substr(2), '\t')) {
                    const std::string t = trim(f);
                    char *e = nullptr;
                    const double v = t.empty() ? 0.0 : std::strtod(t.c_str(), &e);
                    const bool good = !t.empty() && *e == '\0';
                    s.raw.push_back(t);
                    s.value.push_back(good ? v : 0.0);
                    s.ok.push_back(good);
                }
            }
            found = true;
        } else if (stray && !l.empty()) {
            stray->push_back(l);
        }
    }
    return found;
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
