#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int
create_window(Display *dpy, int screen, Window parent, unsigned int width,
              unsigned int height, Window *window)
{
    Window w;
    XSetWindowAttributes attrs;
    unsigned long valuemask;

    attrs.background_pixel = WhitePixel(dpy, screen);
    attrs.border_pixel = BlackPixel(dpy, screen);
    valuemask = CWBackPixel | CWBorderPixel;

    w = XCreateWindow(dpy, parent, 0, 0, width, height, 1,
                      CopyFromParent, InputOutput, CopyFromParent,
                      valuemask, &attrs);
    if (!w)
        return 0;

    *window = w;
    return 1;
}

static int
set_fixed_size_hints(Display *dpy, Window window, int width, int height)
{
    XSizeHints hints;

    hints.flags = PMinSize | PMaxSize;
    hints.min_width = hints.max_width = width;
    hints.min_height = hints.max_height = height;

    XSetWMNormalHints(dpy, window, &hints);
    return 1;
}

int
main(void)
{
    Display *dpy;
    int screen;
    Window root;
    Window primary = None;
    Window transient = None;
    XEvent event;

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "transient: cannot open X display\n");
        return EXIT_FAILURE;
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    if (!create_window(dpy, screen, root, 400, 400, &primary)) {
        fprintf(stderr, "transient: cannot create primary window\n");
        XCloseDisplay(dpy);
        return EXIT_FAILURE;
    }

    if (!set_fixed_size_hints(dpy, primary, 400, 400)) {
        fprintf(stderr, "transient: cannot set primary window size hints\n");
        XDestroyWindow(dpy, primary);
        XCloseDisplay(dpy);
        return EXIT_FAILURE;
    }

    XStoreName(dpy, primary, "swm transient test");
    XSelectInput(dpy, primary, ExposureMask | StructureNotifyMask);
    XMapWindow(dpy, primary);
    XFlush(dpy);

    sleep(5);

    if (!create_window(dpy, screen, root, 100, 100, &transient)) {
        fprintf(stderr, "transient: cannot create transient window\n");
        XDestroyWindow(dpy, primary);
        XCloseDisplay(dpy);
        return EXIT_FAILURE;
    }

    if (!XSetTransientForHint(dpy, transient, primary)) {
        fprintf(stderr, "transient: cannot set WM_TRANSIENT_FOR\n");
        XDestroyWindow(dpy, transient);
        XDestroyWindow(dpy, primary);
        XCloseDisplay(dpy);
        return EXIT_FAILURE;
    }

    XStoreName(dpy, transient, "swm transient child");
    XSelectInput(dpy, transient, ExposureMask | StructureNotifyMask);
    XMapWindow(dpy, transient);
    XFlush(dpy);

    for (;;) {
        XNextEvent(dpy, &event);
    }

    /* Unreachable during normal operation. */
    XDestroyWindow(dpy, transient);
    XDestroyWindow(dpy, primary);
    XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}
