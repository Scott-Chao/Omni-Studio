#include "core/mainwindow.h"
#include "config/configmanager.h"
#include "core/thememanager.h"

#include <QApplication>
#include <QStyleFactory>
#include <QProxyStyle>
#include <QLocale>
#include <QTranslator>
#include <QIcon>
#include <QWebEngineView>

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

#if defined(Q_OS_LINUX)
    // 预热 Qt WebEngine（Linux）：创建一个 QWebEngineView 作为
    // 主窗口的子控件，
    // 使其在首次映射时创建 Wayland 子表面并初始化 Chromium 的 EGL 上下文。
    // 子表面创建+EGL 握手的闪屏代价在启动阶段支付，而非用户首次预览时。
    {
        auto *warmup = new QWebEngineView(&w);
        warmup->resize(1, 1);
        warmup->move(-1, -1);
        warmup->page()->load(QUrl(QStringLiteral("about:blank")));
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        // 热视图作为 MainWindow 子控件持续存活，不析构
    }
#elif defined(Q_OS_MACOS)
    // 预热 Qt WebEngine（macOS）：创建一个 QWebEngineView 并强制创建原生
    // NSView，确保 WebEngine 子进程在首个预览加载前完成初始化。
    {
        auto *warmup = new QWebEngineView(&w);
        warmup->resize(1, 1);
        warmup->move(-1, -1);
        warmup->winId(); // 强制创建 NSView，触发 WebEngine 进程初始化
        warmup->page()->load(QUrl(QStringLiteral("about:blank")));
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
#endif
    w.show();
    return a.exec();
}
