#pragma once
#include <QString>

#include "hwmoncore.h"

// What the user edits (text), kept in QSettings group "hwmon". The page edits and saves it, the session
// reads it when it starts and on "reload".
struct HwmonSettings {
    int periodMs = 100;
    bool precise = true;   // timestamps from $EPOCHREALTIME on the device (bash 5); false = simple mode
    QString sensorsText;
    QString infoText;

    static HwmonSettings load();
    void save() const;
    // Parses the texts; on failure `error` says what is wrong.
    bool toConfig(hwmon::Config &cfg, QString *error) const;
};
