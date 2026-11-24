#include "hwmonsettings.h"

#include <QCoreApplication>
#include <QSettings>

HwmonSettings HwmonSettings::load()
{
    QSettings s;
    s.beginGroup(QStringLiteral("hwmon"));
    HwmonSettings h;
    h.periodMs = qBound(hwmon::kMinPeriodMs, s.value("period", 100).toInt(), hwmon::kMaxPeriodMs);
    h.sensorsText = s.value("sensors", QString::fromStdString(hwmon::defaultSensorsText())).toString();
    h.infoText = s.value("info", QString::fromStdString(hwmon::defaultInfoText())).toString();
    return h;
}

void HwmonSettings::save() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("hwmon"));
    s.setValue("period", periodMs);
    s.setValue("sensors", sensorsText);
    s.setValue("info", infoText);
}

bool HwmonSettings::toConfig(hwmon::Config &cfg, QString *error) const
{
    std::string err;
    cfg = hwmon::Config();
    cfg.periodMs = periodMs;
    if (!hwmon::parseSensors(sensorsText.toStdString(), cfg.sensors, err)) {
        if (error)
            *error = QCoreApplication::translate("HwmonSettings", "Sensors: %1").arg(QString::fromStdString(err));
        return false;
    }
    if (!hwmon::parseInfos(infoText.toStdString(), cfg.infos, err)) {
        if (error)
            *error = QCoreApplication::translate("HwmonSettings", "Info commands: %1").arg(QString::fromStdString(err));
        return false;
    }
    return true;
}
