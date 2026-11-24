#pragma once
#include <QByteArray>
#include <QList>
#include <QString>

// A named set of hwmon settings: sensors, info commands, period. Stored as a small JSON file
// so that it can be saved, copied to a colleague or kept in a repository:
//   {"format":"iwp-hwmon-preset","version":1,"name":"...","description":"...",
//    "periodMs":100,"sensors":"<the text of the sensors list>","info":"<the text of the commands>"}
// The two lists are kept as the text the user edits (with the comments).
struct HwmonPreset {
    QString name;
    QString description;
    int periodMs = 100;
    QString sensorsText;
    QString infoText;
    QString filePath;   // empty for the built-in ones

    QByteArray toJson() const;
    static bool fromJson(const QByteArray &data, HwmonPreset &out, QString *error);

    static QList<HwmonPreset> builtins();
    static QString userDir();                    // where "Save" proposes to put the files
    static QList<HwmonPreset> userPresets();     // the valid *.json files of userDir()
    static QString fileNameFor(const QString &name);
};
