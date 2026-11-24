// Run from the repo root:
//   g++ -std=c++17 -Wall -Wextra -Iplugins/device/hwmon tools/hwmoncore_test.cpp -o /tmp/hwmoncore_test && /tmp/hwmoncore_test
// The end-to-end part runs the generated scripts in real shells (sh, bash, busybox ash if present) against fake sysfs files.
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
        auto L = [](const char *d, const char *label, const char *path) {
            return std::vector<std::string>{d, label, path};
        };
        auto sl = makeSlots(c, {L("2", "x1/y", "/a/x1/y"), L("2", "x2/y", "/a/x2/y"), L("0", "", "/a/b"), L("9", "", "/a/b"),
                                L("1", "", "/a/*/unmatched"), std::vector<std::string>{"junk"}});
        CHECK(sl.size() == 4 && sl[0].name == "rx1/y" && sl[0].path == "/a/x1/y" && sl[1].name == "rx2/y" && sl[2].name == "t" && sl[2].def == 0);
        CHECK(sl[3].path.empty());   // an unmatched glob: the slot stays, nothing to read
        CHECK(declaredSlots(c).size() == 3);
    }
    for (const BuiltinPreset &b : builtinPresets()) {
        std::vector<Sensor> ss; std::vector<Info> ii; std::string err;
        CHECK(parseSensors(b.sensors, ss, err));
        CHECK(parseInfos(b.info, ii, err));
        CHECK(b.periodMs >= kMinPeriodMs && b.periodMs <= kMaxPeriodMs);
    }
    CHECK(builtinPresets().size() >= 3);

    // ---- end to end: the scripts in real shells against fake sysfs files ----
    const std::string sh0 = "sh";
    std::vector<std::string> shells = {"sh", "bash"};
    if (system("busybox ash -c true 2>/dev/null") == 0)
        shells.push_back("busybox ash");
    char tmpl[] = "/tmp/hwmontestXXXXXX";
    const std::string dir = mkdtemp(tmpl);
    CHECK(system(("mkdir -p " + dir + "/hwmon7 " + dir + "/z0 " + dir + "/z1 " + dir + "/z2 " + dir + "/many").c_str()) == 0);
    { std::ofstream(dir + "/hwmon7/curr1_input") << "123"; }          // no newline at the end, like sysfs sometimes
    { std::ofstream(dir + "/hwmon7/curr2_input") << "456\n"; }
    { std::ofstream(dir + "/z0/temp") << "41000\n"; }
    { std::ofstream(dir + "/z1/temp") << "42500\n"; }
    { std::ofstream(dir + "/z2/temp") << "43000\n"; }
    { std::ofstream(dir + "/state") << "RUN\tmode two\n"; }
    for (int i = 0; i < 40; ++i) { std::ofstream(dir + "/many/f" + std::to_string(i)) << i << "\n"; }

    // what the "ssh" endpoint does with a command: ( eval '<command>' ) </dev/null 2>&1
    auto run = [&](const std::string &shell, const std::string &script) {
        { std::ofstream(dir + "/cmd") << "( eval " << shQuote(script) << " ) </dev/null 2>&1\n"; }
        const std::string cmd = shell + " < " + dir + "/cmd > " + dir + "/out";
        if (system(cmd.c_str()) != 0)
            return std::string("<failed>");
        return readFile(dir + "/out");
    };

    for (const std::string &shell : shells) {
        printf("shell: %s\n", shell.c_str());
        Config cfg;
        cfg.periodMs = 100;
        cfg.sensors = {mk("c1", dir + "/hwmon*/curr1_input", 1, "mA"), mk("c2", dir + "/hwmon*/curr2_input", 1, "mA"),
                       mk("missing", dir + "/hwmon*/nothing_input", 1, ""), mk("zone {}", dir + "/z*/temp", 1000, "C"),
                       mk("state", dir + "/state", 1, ""), mk("none {}", dir + "/nothing*/x", 1, ""),
                       mk("tail", dir + "/z0/temp", 1000, "C")};
        cfg.sensors[4].text = true;
        cfg.infos = {{"One", "printf 'line1\\nline2\\\\x'"}, {"Two", "echo \"it's fine\""}, {"Bad", "exit 3"}};
        const std::string out = run(shell, buildDiscoveryScript(cfg));
        CHECK(out != "<failed>");
        const Discovery d = parseDiscovery(cfg, out);
        CHECK(d.info.size() == 3 && d.info[0] == "line1\nline2\\x" && d.info[1] == "it's fine" && d.info[2].empty());
        CHECK(d.errors.size() == 2);   // "missing" (no readable file) and "none {}" (no file matches)
        const auto slots = makeSlots(cfg, d.slotMsgs);
        // c1, c2, missing (kept, nothing to read), zone x3, state, tail  (none {} gives no slot)
        CHECK(slots.size() == 8);
        CHECK(slots[3].name == "zone z0/temp" && slots[5].name == "zone z2/temp" && slots[6].name == "state" && slots[7].name == "tail");
        CHECK(slots[0].path == dir + "/hwmon7/curr1_input" && slots[2].path.empty());
        CHECK(d.stray.empty());

        const std::string poll = buildPollScript(slots);
        const std::string reply = run(shell, poll);
        Sample sm;
        std::vector<std::string> stray;
        CHECK(parseSample(reply, sm, &stray) && stray.empty());
        CHECK(sm.value.size() == 8);
        CHECK(sm.ok[0] && sm.value[0] == 123 && sm.ok[1] && sm.value[1] == 456 && !sm.ok[2]);
        CHECK(sm.value[3] == 41000 && sm.value[5] == 43000 && sm.value[7] == 41000);
        CHECK(sm.raw[6] == "RUN mode two");   // the tab inside the text did not shift the columns
    }

    // the cap of kMaxSensors slots
    for (const std::string &shell : shells) {
        Config c3;
        c3.sensors = {mk("f{}", dir + "/many/f*", 1, ""), mk("tail", dir + "/many/f0", 1, "")};
        const Discovery d = parseDiscovery(c3, run(shell, buildDiscoveryScript(c3)));
        const auto slots = makeSlots(c3, d.slotMsgs);
        CHECK(int(slots.size()) == kMaxSensors);
        Sample sm;
        CHECK(parseSample(run(shell, buildPollScript(slots)), sm) && sm.value.size() == size_t(kMaxSensors));
    }

    // an answer with noise around the data (banner, shell messages)
    {
        Sample sm; std::vector<std::string> stray;
        CHECK(parseSample("Welcome\nIPv6:\nS\t1\t\t3\nroot@host:~# \n", sm, &stray));
        CHECK(sm.value.size() == 3 && sm.ok[0] && !sm.ok[1] && sm.ok[2] && stray.size() == 3);
        CHECK(!parseSample("nothing here\n", sm, nullptr));
        const Discovery d = parseDiscovery(Config(), "banner\nE\tsensor x: no readable file\nL\t0\t\t/a/b\n");
        CHECK(d.stray.size() == 1 && d.errors.size() == 1 && d.slotMsgs.size() == 1 && d.slotMsgs[0].size() == 3);
    }

    CHECK(system(("rm -rf " + dir).c_str()) == 0);
    printf("ALL OK\n");
    return 0;
}
