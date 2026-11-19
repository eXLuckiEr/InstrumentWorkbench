#include "linuxhostsession.h"

static const QString kShell = QStringLiteral("shell");

// Everything the device has to answer in one go. Plain POSIX sh + awk, works on busybox too.
static const char kSensorScript[] = R"SH(
echo "load1=$(cut -d' ' -f1 /proc/loadavg)"
echo "uptime_s=$(cut -d. -f1 /proc/uptime)"
awk '/^MemTotal:/{print "mem_total_kb="$2} /^MemAvailable:/{print "mem_avail_kb="$2}' /proc/meminfo
for z in /sys/class/thermal/thermal_zone*; do
  [ -r "$z/temp" ] && echo "temp/$(cat "$z/type" 2>/dev/null)/$(basename "$z")=$(cat "$z/temp")"
done
for h in /sys/class/hwmon/hwmon*; do
  n=$(cat "$h/name" 2>/dev/null)
  for t in "$h"/temp*_input; do
    [ -r "$t" ] && echo "hwmon/$n/$(basename "$t" _input)=$(cat "$t")"
  done
done
true
)SH";

LinuxHostSession::LinuxHostSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent)
    : SessionBase(endpoints, model, parent)
{
    poll("sensors", 2000, kShell, QByteArray(kSensorScript), [this](const iwp::Response &r) {
        if (r.ok)
            parseLines(r.data);
    });
}

void LinuxHostSession::onStart()
{
    query(kShell, "echo \"os=$(uname -sr)\"; echo \"hostname=$(hostname)\"", [this](const iwp::Response &r) {
        if (!r.ok) {
            fail(tr("Cannot run commands: %1").arg(r.error));
            return;
        }
        parseLines(r.data);
        setRunning();
    }, 0, 10000);
}

void LinuxHostSession::parseLines(const QByteArray &text)
{
    QVariantMap values;
    for (const QByteArray &line : text.split('\n')) {
        const int eq = line.indexOf('=');
        if (eq <= 0)
            continue;
        const QString key = QString::fromUtf8(line.left(eq)).trimmed();
        const QString raw = QString::fromUtf8(line.mid(eq + 1)).trimmed();

        bool isNumber = false;
        double number = raw.toDouble(&isNumber);
        if (!isNumber) {
            values.insert(key, raw);   // text such as os, hostname
            continue;
        }
        // thermal zones and hwmon report millidegrees
        if (key.startsWith(QLatin1String("temp/")) || key.contains(QLatin1String("/temp")))
            number /= 1000.0;
        values.insert(key, number);
    }
    model()->updateMany(values, kShell);
}
