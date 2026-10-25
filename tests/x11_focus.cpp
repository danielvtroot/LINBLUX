#include <X11/Xlib.h>

// Independent display connection: force real GLFW focus-loss callbacks without
// involving a window manager. No X11 code is linked into the application.
unsigned long InputTestRootWindow() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) return 0;
    const Window root = DefaultRootWindow(display);
    XCloseDisplay(display);
    return root;
}
