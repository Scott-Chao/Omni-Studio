#include "mainwindow.h"
#include "configmanager.h"
#include "thememanager.h"

#include <QApplication>
#include <QStyleFactory>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    ConfigManager::instance().load();

    qputenv("QTWEBENGINE_REMOTE_DEBUGGING",
            ConfigManager::instance().webEngineDebuggingPort().toUtf8());

    QApplication a(argc, argv);

    // Fusion style for consistent cross-platform rendering and proper dark-theme QSS support
    a.setStyle(QStyleFactory::create("Fusion"));

    // Initialize ThemeManager (loads default theme)
    ThemeManager::instance();

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = ConfigManager::instance().translationPrefix()
                                 + QLocale(locale).name();
        if (translator.load(ConfigManager::instance().translationPath() + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    // Load global QSS with theme color tokens
    ThemeManager::instance().loadQss();

    MainWindow w;
    w.show();
    return a.exec();
}
