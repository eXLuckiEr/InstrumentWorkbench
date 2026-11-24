// Run from the repo root:
//   g++ -std=c++17 -Wall -Wextra -Iplugins/device/hwmon tools/hwmoncore_test.cpp -o /tmp/hwmoncore_test && /tmp/hwmoncore_test
// The end-to-end part runs the generated script with a real bash 5 against fake sysfs files (skipped without bash 5).
#include "hwmoncore.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n",__LINE__,#c); return 1;} }while(0)
using namespace hwmon;

static std::string readFile(const std::string &p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }

static hwmon::Sensor mk(const std::string &name, const std::string &path, double div, const std::string &unit)
{
    hwmon::Sensor x;
    x.name = name; x.path = path; x.divisor = div; x.unit = unit;
    return x;
}

int main()
{
    std::string err;
    std::vector<Sensor> sensors;
    CHECK(parseSensors(defaultSensorsText(), sensors, err));
    CHECK(sensors.size() == 3 && sensors[0].name == "curr1" && sensors[0].unit == "mA" && sensors[2].divisor == 1.0);
    CHECK(sensors[0].path == "/sys/bus/i2c/devices/13-0048/hwmon/hwmon*/curr1_input");
    CHECK(!parseSensors("x | relative/path", sensors, err));
    CHECK(!parseSensors("x | /a b", sensors, err));
    CHECK(!parseSensors("x | /a;rm", sensors, err));
    CHECK(!parseSensors("x | /a | 0", sensors, err));
    CHECK(!parseSensors("x | /a | abc", sensors, err));
    CHECK(!parseSensors(" | /a", sensors, err));
    CHECK(parseSensors("v | /a/b | 1000 | V\n\n# c\n", sensors, err) && sensors.size() == 1 && sensors[0].divisor == 1000);
    std::string many;
    for (int i = 0; i < 33; ++i) many += "s" + std::to_string(i) + " | /a/b\n";
    CHECK(!parseSensors(many, sensors, err));

    std::vector<Info> infos;
    CHECK(parseInfos(defaultInfoText(), infos, err) && infos.size() == 2 && infos[0].name == "FPGA");
    CHECK(parseInfos("x | cat a | head -1", infos, err) && infos[0].command == "cat a | head -1");
    CHECK(!parseInfos("only-name", infos, err));

    CHECK(shQuote("a'b") == "'a'\\''b'");
    CHECK(decimalsFor(1) == 0 && decimalsFor(10) == 1 && decimalsFor(1000) == 3 && decimalsFor(1e6) == 6);

    // lines and messages
    LineBuffer lb; std::vector<std::string> lines;
    const std::string chunk1 = "S\t100\t1\t2\r\nI\tFPGA\tv1\\nv2\nS\t10";
    lb.feed(chunk1.data(), chunk1.size(), lines);
    CHECK(lines.size() == 2);
    lines.clear();
    const std::string chunk2 = "1\t3\n";
    lb.feed(chunk2.data(), chunk2.size(), lines);
    CHECK(lines.size() == 1 && lines[0] == "S\t101\t3");
    Message m;
    CHECK(parseMessage("H", m) && m.type == 'H');
    CHECK(parseMessage("I\tFPGA\ta\tb", m) && m.type == 'I' && m.fields.size() == 2 && m.fields[1] == "a\tb");
    CHECK(!parseMessage("garbage", m) && !parseMessage("X\tfoo", m));
    CHECK(unescapeInfo("a\\nb\\\\c") == "a\nb\\c");
    Sample s;
    CHECK(parseMessage("S\t1732450123456789\t12\t\tabc\t-5", m) && parseSample(m.fields, s));
    CHECK(s.tUs == 1732450123456789LL && s.value.size() == 4 && s.ok[0] && !s.ok[1] && !s.ok[2] && s.ok[3] && s.value[3] == -5);
    CHECK(!parseSample({"0"}, s));
    CHECK(parseSample({"-", "7", ""}, s) && s.tUs == 0 && s.value.size() == 2 && s.ok[0] && !s.ok[1]);
    {
        Config c; c.periodMs = 250; c.precise = false;
        const std::string sc = buildScript(c);
        CHECK(sc.find("EPOCHREALTIME") == std::string::npos && sc.find("read -r -t 0.250 c") != std::string::npos);
        c.precise = true;
        CHECK(buildScript(c).find("EPOCHREALTIME") != std::string::npos);
    }

    // ---- options, masks ----
    {
        std::vector<Sensor> ss; std::string err;
        CHECK(parseSensors("t | /a/b | 1000 | C | min=10 max=85.5\nst | /a/s | 1 | | text expect=OK\nr{} | /a/x*/y | 1\n", ss, err));
        CHECK(ss.size() == 3 && ss[0].hasMin && ss[0].minV == 10 && ss[0].hasMax && ss[0].maxV == 85.5 && !ss[0].text);
        CHECK(ss[1].text && ss[1].expect == "OK" && isMask(ss[2]) && !isMask(ss[0]));
        CHECK(numberAlarm(ss[0], 9.9) && numberAlarm(ss[0], 86) && !numberAlarm(ss[0], 50));
        CHECK(textAlarm(ss[1], "BAD") && !textAlarm(ss[1], "OK"));
        Config c; c.sensors = ss;   // before the negative tests: a failed parse clears the list
        CHECK(!parseSensors("a | /a | 1 | | bogus", ss, err));
        CHECK(!parseSensors("a | /a | 1 | | min=x", ss, err));
        CHECK(!parseSensors("a | /a | 1 | | text min=1", ss, err));
        CHECK(!parseSensors("a | /a | 1 | | expect=OK", ss, err));
        CHECK(!parseSensors("a | /a | 1 | | min=5 max=1", ss, err));
        CHECK(!parseSensors("a | /a | 1 | | text | more", ss, err));
        CHECK(staticPrefixLen("/sys/class/hwmon/hwmon*/temp1") == 17 && staticPrefixLen("/a/b") == 0 && staticPrefixLen("/a/b*") == 3);
        auto sl = makeSlots(c, {{std::string("2"), std::string("x1/y")}, {std::string("2"), std::string("x2/y")},
                                  {std::string("0"), std::string()}, {std::string("9"), std::string()}, {std::string("junk")}});
        CHECK(sl.size() == 3 && sl[0].name == "rx1/y" && sl[1].name == "rx2/y" && sl[2].name == "t" && sl[2].def == 0);
        CHECK(declaredSlots(c).size() == 3);
    }
    for (const BuiltinPreset &b : builtinPresets()) {
        std::vector<Sensor> ss; std::vector<Info> ii; std::string err;
        CHECK(parseSensors(b.sensors, ss, err));
        CHECK(parseInfos(b.info, ii, err));
        CHECK(b.periodMs >= kMinPeriodMs && b.periodMs <= kMaxPeriodMs);
    }
    CHECK(builtinPresets().size() >= 3);

    // ---- end to end ----
    if (system("bash -c '[[ -n ${EPOCHREALTIME-} ]]' 2>/dev/null") != 0) {
        printf("bash 5 not found: end-to-end part skipped\n");
        printf("ALL OK\n");
        return 0;
    }
    char tmpl[] = "/tmp/hwmontestXXXXXX";
    const std::string dir = mkdtemp(tmpl);
    CHECK(system(("mkdir -p " + dir + "/hwmon7").c_str()) == 0);
    { std::ofstream(dir + "/hwmon7/curr1_input") << "123\n"; }
    { std::ofstream(dir + "/hwmon7/curr2_input") << "456\n"; }
    Config cfg;
    cfg.periodMs = 100;
    cfg.sensors = {mk("c1", dir + "/hwmon*/curr1_input", 1, "mA"), mk("c2", dir + "/hwmon*/curr2_input", 1, "mA"),
                   mk("missing", dir + "/hwmon*/nothing_input", 1, "")};
    cfg.infos = {{"One", "printf 'line1\\nline2\\\\x'"}, {"Two", "echo \"it's fine\""}, {"Bad", "exit 3"}};
    const std::string script = buildScript(cfg);
    { std::ofstream(dir + "/in") << frameScript(script); }
    const std::string cmd = "( cat " + dir + "/in; sleep 1; printf 'STOP\\n' ) | bash -c '"
        "while IFS= read -r n && IFS= read -r -N \"$n\" s; do eval \"$s\"; done' > " + dir + "/out 2>" + dir + "/err";
    CHECK(system(cmd.c_str()) == 0);
    const std::string out = readFile(dir + "/out");
    LineBuffer lb2; std::vector<std::string> ls;
    lb2.feed(out.data(), out.size(), ls);
    int infoCount = 0, errors = 0, ready = 0, samples = 0;
    long long prev = 0, maxGap = 0;
    for (const std::string &l : ls) {
        Message mm;
        if (!parseMessage(l, mm)) continue;
        if (mm.type == 'I') {
            ++infoCount;
            if (mm.fields[0] == "One") CHECK(unescapeInfo(mm.fields[1]) == "line1\nline2\\x");
            if (mm.fields[0] == "Two") CHECK(unescapeInfo(mm.fields[1]) == "it's fine");
        } else if (mm.type == 'E') {
            ++errors;
        } else if (mm.type == 'R') {
            ++ready;
            CHECK(mm.fields.size() == 2 && mm.fields[0] == "3" && mm.fields[1] == "100000");
            CHECK(infoCount == 3 && samples == 0);   // info first, then ready, then samples
        } else if (mm.type == 'S') {
            Sample sm;
            CHECK(parseSample(mm.fields, sm) && sm.value.size() == 3);
            CHECK(sm.ok[0] && sm.value[0] == 123 && sm.ok[1] && sm.value[1] == 456 && !sm.ok[2]);
            if (prev) maxGap = std::max(maxGap, sm.tUs - prev);
            if (prev) CHECK(sm.tUs > prev);
            prev = sm.tUs;
            ++samples;
        }
    }
    printf("info %d, errors %d, samples %d, max gap %lld us\n", infoCount, errors, samples, maxGap);
    CHECK(infoCount == 3 && errors == 1 && ready == 1);
    CHECK(samples >= 7 && samples <= 12);        // 1 s at 100 ms
    CHECK(maxGap < 160000);
    // ---- masks and text sensors, end to end ----
    {
        CHECK(system(("mkdir -p " + dir + "/z0 " + dir + "/z1 " + dir + "/z2").c_str()) == 0);
        { std::ofstream(dir + "/z0/temp") << "41000\n"; }
        { std::ofstream(dir + "/z1/temp") << "42500\n"; }
        { std::ofstream(dir + "/z2/temp") << "43000\n"; }
        { std::ofstream(dir + "/state") << "RUN\tmode two\n"; }
        Config c2;
        c2.periodMs = 50;
        c2.sensors = {mk("zone {}", dir + "/z*/temp", 1000, "C"), mk("state", dir + "/state", 1, ""),
                      mk("none {}", dir + "/nothing*/x", 1, ""), mk("last", dir + "/z0/temp", 1000, "C")};
        c2.sensors[1].text = true;
        { std::ofstream(dir + "/in3") << frameScript(buildScript(c2)); }
        const std::string cmd3 = "( cat " + dir + "/in3; sleep 0.4; printf 'STOP\\n' ) | bash -c '"
            "while IFS= read -r n && IFS= read -r -N \"$n\" s; do eval \"$s\"; done' > " + dir + "/out3 2>" + dir + "/err3";
        CHECK(system(cmd3.c_str()) == 0);
        const std::string o3 = readFile(dir + "/out3");
        LineBuffer lb4; std::vector<std::string> l3;
        lb4.feed(o3.data(), o3.size(), l3);
        std::vector<std::vector<std::string>> lm;
        int e3 = 0, n3 = 0;
        for (const std::string &l : l3) {
            Message mm;
            if (!parseMessage(l, mm)) continue;
            if (mm.type == 'L') lm.push_back(mm.fields);
            if (mm.type == 'E') ++e3;
            if (mm.type == 'S') {
                Sample sm;
                CHECK(parseSample(mm.fields, sm) && sm.value.size() == 5);   // 3 zones + state + (none: no slot) + last
                CHECK(sm.value[0] == 41000 && sm.value[2] == 43000 && sm.raw[3] == "RUN mode two" && sm.value[4] == 41000 + 0);
                ++n3;
            }
        }
        const auto sl = makeSlots(c2, lm);
        printf("masks: slots %zu, errors %d, samples %d\n", sl.size(), e3, n3);
        CHECK(sl.size() == 5 && sl[0].name == "zone z0/temp" && sl[2].name == "zone z2/temp" && sl[3].name == "state" && sl[4].name == "last");
        CHECK(e3 == 1 && n3 >= 4);   // "none {}" matches nothing: one error, no slot
    }

    // ---- the cap of 32 slots ----
    {
        CHECK(system(("mkdir -p " + dir + "/many").c_str()) == 0);
        for (int i = 0; i < 40; ++i) { std::ofstream(dir + "/many/f" + std::to_string(i)) << i << "\n"; }
        Config c3; c3.periodMs = 100;
        c3.sensors = {mk("f{}", dir + "/many/f*", 1, ""), mk("tail", dir + "/many/f0", 1, "")};
        { std::ofstream(dir + "/in4") << frameScript(buildScript(c3)); }
        const std::string cmd4 = "( cat " + dir + "/in4; sleep 0.3; printf 'STOP\\n' ) | bash -c '"
            "while IFS= read -r n && IFS= read -r -N \"$n\" s; do eval \"$s\"; done' > " + dir + "/out4 2>/dev/null";
        CHECK(system(cmd4.c_str()) == 0);
        const std::string o4 = readFile(dir + "/out4");
        LineBuffer lb5; std::vector<std::string> l4;
        lb5.feed(o4.data(), o4.size(), l4);
        int ls4 = 0; size_t width = 0;
        for (const std::string &l : l4) {
            Message mm;
            if (!parseMessage(l, mm)) continue;
            if (mm.type == 'L') ++ls4;
            if (mm.type == 'S') { Sample sm; CHECK(parseSample(mm.fields, sm)); width = sm.value.size(); }
        }
        printf("cap: %d slots, sample width %zu\n", ls4, width);
        CHECK(ls4 == kMaxSensors && width == size_t(kMaxSensors));
    }

    // ---- the simple mode (no timestamps from the device) ----
    {
        cfg.precise = false;
        { std::ofstream(dir + "/in2") << frameScript(buildScript(cfg)); }
        const std::string cmd2 = "( cat " + dir + "/in2; sleep 1; printf 'STOP\\n' ) | bash -c '"
            "while IFS= read -r n && IFS= read -r -N \"$n\" s; do eval \"$s\"; done' > " + dir + "/out2 2>" + dir + "/err2";
        CHECK(system(cmd2.c_str()) == 0);
        const std::string o2 = readFile(dir + "/out2");
        LineBuffer lb3; std::vector<std::string> l2;
        lb3.feed(o2.data(), o2.size(), l2);
        int n2 = 0, inf2 = 0;
        for (const std::string &l : l2) {
            Message mm;
            if (!parseMessage(l, mm)) continue;
            if (mm.type == 'I') ++inf2;
            if (mm.type == 'S') {
                Sample sm;
                CHECK(parseSample(mm.fields, sm) && sm.tUs == 0 && sm.value.size() == 3);
                CHECK(sm.ok[0] && sm.value[0] == 123 && sm.ok[1] && sm.value[1] == 456 && !sm.ok[2]);
                ++n2;
            }
        }
        printf("simple mode: info %d, samples %d\n", inf2, n2);
        CHECK(inf2 == 3 && n2 >= 5 && n2 <= 11);   // a pause of 100 ms + the reading time
    }
    // STOP returned to the outer loop and the process ended at EOF: nothing left over
    CHECK(system(("rm -rf " + dir).c_str()) == 0);
    printf("ALL OK\n");
    return 0;
}
