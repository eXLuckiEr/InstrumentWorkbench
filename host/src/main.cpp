#include <QApplication>
#include <QDir>
#include <QFileInfo>

#include "appsettings.h"
#include "iwp/icons.h"
#include "mainwindow.h"
#include "pluginmanager.h"
#include "translations.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("IWP");
    QApplication::setApplicationName("InstrumentWorkbenchPlatform");

    QApplication::setWindowIcon(iwp::svgIcon(QStringLiteral(":/iwp/host/app.svg")));
    AppSettings::init(app.arguments());

    // Host and SDK strings first, so that even plugin loading errors are translated.
    Translations::installCore(Translations::requestedLanguage(app.arguments()));

    PluginManager plugins;
    const QString appDir = QCoreApplication::applicationDirPath();
    plugins.loadFrom(appDir + "/plugins");
#ifdef Q_OS_MACOS
    plugins.loadFrom(QDir::cleanPath(appDir + "/../PlugIns"));   // inside .app bundle
#endif

    // Every loaded plugin brings its own translations.
    for (const PluginManager::Info &info : plugins.infos()) {
        if (!info.loaded)
            continue;
        QString module = QFileInfo(info.file).completeBaseName();
        if (module.startsWith(QLatin1String("lib")))
            module.remove(0, 3);
        Translations::installModule(module);
    }

    MainWindow w(plugins);
    w.show();
    return app.exec();
}
