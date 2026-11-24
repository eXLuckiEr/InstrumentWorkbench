#include "hwmonpreset.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

#include "hwmoncore.h"

namespace {
const QString kFormat = QStringLiteral("iwp-hwmon-preset");
}

QByteArray HwmonPreset::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("format")] = kFormat;
    o[QStringLiteral("version")] = 1;
    o[QStringLiteral("name")] = name;
    o[QStringLiteral("description")] = description;
    o[QStringLiteral("periodMs")] = periodMs;
    o[QStringLiteral("sensors")] = sensorsText;
    o[QStringLiteral("info")] = infoText;
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

bool HwmonPreset::fromJson(const QByteArray &data, HwmonPreset &out, QString *error)
{
    auto fail = [&](const QString &m) {
        if (error)
            *error = m;
        return false;
    };
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (doc.isNull() || !doc.isObject())
        return fail(QCoreApplication::translate("HwmonPreset", "not a JSON file: %1").arg(pe.errorString()));
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("format")).toString() != kFormat)
        return fail(QCoreApplication::translate("HwmonPreset", "this is not an hwmon preset"));
    if (o.value(QStringLiteral("version")).toInt() != 1)
        return fail(QCoreApplication::translate("HwmonPreset", "unsupported preset version"));
    HwmonPreset p;
    p.name = o.value(QStringLiteral("name")).toString();
    p.description = o.value(QStringLiteral("description")).toString();
    p.periodMs = qBound(hwmon::kMinPeriodMs, o.value(QStringLiteral("periodMs")).toInt(100), hwmon::kMaxPeriodMs);
    p.sensorsText = o.value(QStringLiteral("sensors")).toString();
    p.infoText = o.value(QStringLiteral("info")).toString();
    // The lists must be valid: a broken preset is better refused here than at "Apply".
    std::vector<hwmon::Sensor> sensors;
    std::vector<hwmon::Info> infos;
    std::string err;
    if (!hwmon::parseSensors(p.sensorsText.toStdString(), sensors, err))
        return fail(QCoreApplication::translate("HwmonPreset", "sensors: %1").arg(QString::fromStdString(err)));
    if (!hwmon::parseInfos(p.infoText.toStdString(), infos, err))
        return fail(QCoreApplication::translate("HwmonPreset", "commands: %1").arg(QString::fromStdString(err)));
    out = p;
    return true;
}

QList<HwmonPreset> HwmonPreset::builtins()
{
    QList<HwmonPreset> list;
    for (const hwmon::BuiltinPreset &b : hwmon::builtinPresets()) {
        HwmonPreset p;
        p.name = QString::fromStdString(b.name);
        p.description = QString::fromStdString(b.description);
        p.periodMs = b.periodMs;
        p.sensorsText = QString::fromStdString(b.sensors);
        p.infoText = QString::fromStdString(b.info);
        list.append(p);
    }
    return list;
}

QString HwmonPreset::userDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/hwmon-presets");
}

QList<HwmonPreset> HwmonPreset::userPresets()
{
    QList<HwmonPreset> list;
    const QDir dir(userDir());
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QString &f : files) {
        QFile file(dir.filePath(f));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        HwmonPreset p;
        if (!fromJson(file.read(1 << 20), p, nullptr))
            continue;
        if (p.name.isEmpty())
            p.name = QFileInfo(f).completeBaseName();
        p.filePath = file.fileName();
        list.append(p);
    }
    return list;
}

QString HwmonPreset::fileNameFor(const QString &name)
{
    QString n = name.trimmed();
    n.replace(QRegularExpression(QStringLiteral("[^\\w\\-. ]+")), QStringLiteral("_"));
    if (n.isEmpty())
        n = QStringLiteral("preset");
    return n + QStringLiteral(".json");
}
