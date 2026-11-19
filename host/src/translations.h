#pragma once
#include <QString>
#include <QStringList>

// UI language of the application.
//   "en"      English, the source language of all strings (default, nothing is loaded)
//   "system"  language of the operating system
//   "ru", ... any language for which modules have i18n/<module>_<lang>.ts
//
// Chosen (first match wins): --lang <code>, environment IWP_LANG,
// the settings file (ui/language), otherwise "en".
namespace Translations {

QString requestedLanguage(const QStringList &arguments);

// Host, SDK and Qt's own strings (dialog buttons...). Call before anything is shown.
void installCore(const QString &language);

// Strings of one plugin, embedded in the plugin itself (:/i18n/<module>_<lang>.qm).
// moduleName = plugin file name without "lib" prefix and extension, e.g. "iwp_ssh".
void installModule(const QString &moduleName);

} // namespace Translations
