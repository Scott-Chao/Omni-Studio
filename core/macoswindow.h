#ifndef MACOSWINDOW_H
#define MACOSWINDOW_H

#include <QtGlobal>

class QWindow;

namespace MacOSWindow {
    void enableFullSizeContentView(QWindow *window);
}

#endif // MACOSWINDOW_H
