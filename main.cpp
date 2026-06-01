#include "core/mainwindow.h"
#include "config/configmanager.h"
#include "core/thememanager.h"

#include <QApplication>
#include <QStyleFactory>
#include <QProxyStyle>
#include <QLocale>
#include <QTranslator>
#include <QIcon>

// QProxyStyle that reduces Qt's default tooltip internal margins.
class CompactTooltipStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override
    {
        if (metric == PM_ToolTipLabelFrameWidth)
            return 0;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};

int main(int argc, char *argv[])
{
    ConfigManager::instance().load();

    qputenv("QTWEBENGINE_REMOTE_DEBUGGING",
            ConfigManager::instance().webEngineDebuggingPort().toUtf8());

    QApplication a(argc, argv);

    // Fusion style wrapped with compact tooltip proxy
    a.setStyle(new CompactTooltipStyle(QStyleFactory::create("Fusion")));

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

    QIcon::setFallbackSearchPaths(QIcon::fallbackSearchPaths() << ":/icons");
    a.setWindowIcon(QIcon(":/icons/app"));

    MainWindow w;
    w.show();
    return a.exec();
}
