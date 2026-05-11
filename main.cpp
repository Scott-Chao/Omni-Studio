#include "mainwindow.h"
#include "configmanager.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    ConfigManager::instance().load();

    qputenv("QTWEBENGINE_REMOTE_DEBUGGING",
            ConfigManager::instance().webEngineDebuggingPort().toUtf8());
    QApplication a(argc, argv);

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
    MainWindow w;
    w.show();
    return a.exec();
}
