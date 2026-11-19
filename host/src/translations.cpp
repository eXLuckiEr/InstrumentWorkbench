#include "translations.h"

#include "appsettings.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace {

QLocale g_locale(QLocale::English);   // language chosen by installCore()

bool install(const QString &name, const QString &directory)
{
    auto *translator = new QTranslator(QCoreApplication::instance());
    if (translator->load(g_locale, name, QStringLiteral("_"), directory)) {
        QCoreApplication::installTranslator(translator);
        return true;
    }
    delete translator;
    return false;
}

} // namespace

namespace Translations {

QString requestedLanguage(const QStringList &arguments)
{
    const int i = arguments.indexOf(QStringLiteral("--lang"));
    if (i >= 0 && i + 1 < arguments.size())
        return arguments.at(i + 1);
    const QString env = qEnvironmentVariable("IWP_LANG");
    if (!env.isEmpty())
        return env;
    return AppSettings::instance().language();
}

void installCore(const QString &language)
{
    g_locale = (language == QLatin1String("system")) ? QLocale::system() : QLocale(language);
    if (g_locale.language() == QLocale::English || g_locale.language() == QLocale::C)
        return;   // source language: strings are used as written
    QLocale::setDefault(g_locale);   // number and date formats follow the language

    install(QStringLiteral("iwp_host"), QStringLiteral(":/i18n"));
    install(QStringLiteral("iwp_sdk"), QStringLiteral(":/i18n"));
    install(QStringLiteral("qtbase"), QLibraryInfo::path(QLibraryInfo::TranslationsPath));
}

void installModule(const QString &moduleName)
{
    if (g_locale.language() == QLocale::English || g_locale.language() == QLocale::C)
        return;
    install(moduleName, QStringLiteral(":/i18n"));
}

} // namespace Translations
