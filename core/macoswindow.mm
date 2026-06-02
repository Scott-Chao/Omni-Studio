#include "macoswindow.h"
#include <QWindow>

#if defined(__APPLE__)
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>

namespace MacOSWindow {

void enableFullSizeContentView(QWindow *window)
{
    if (!window)
        return;

    // Obtain the native NSView from Qt's window handle
    NSView *nativeView = (NSView *)(window->winId());
    if (!nativeView)
        return;

    NSWindow *nsWindow = [nativeView window];
    if (!nsWindow)
        return;

    // Make title bar transparent and extend content into it
    // This keeps the native traffic light buttons (close/minimize/maximize)
    // while allowing our custom toolbar to render behind them.
    [nsWindow setTitlebarAppearsTransparent:YES];
    [nsWindow setStyleMask:[nsWindow styleMask] | NSWindowStyleMaskFullSizeContentView];
    [nsWindow setTitleVisibility:NSWindowTitleHidden];
}

} // namespace MacOSWindow
#endif
