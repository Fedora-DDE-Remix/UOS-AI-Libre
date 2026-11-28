// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongxwayland.h"

#include <QGuiApplication>
#include <QScreen>

#if defined(UOS_AI_LIBRE_HAVE_X11_RECORD) \
    || defined(UOS_AI_LIBRE_HAVE_X11_SHORTCUT)
#include <X11/Xlib.h>
// Xlib's unprefixed macros collide with Qt's enum names.
#undef None
#undef Bool
#undef Status
#undef CursorShape
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef Expose
#endif

namespace {

bool coordinateSpaceMatches(Display *display)
{
    const int rootWidth = DisplayWidth(display, DefaultScreen(display));
    const int rootHeight = DisplayHeight(display, DefaultScreen(display));

    QRect virtualGeometry;
    for (QScreen *screen : QGuiApplication::screens())
        virtualGeometry = virtualGeometry.united(screen->virtualGeometry());

    return !virtualGeometry.isEmpty()
        && virtualGeometry.size() == QSize(rootWidth, rootHeight);
}

QRect virtualDesktopGeometry()
{
    QRect geometry;
    for (QScreen *screen : QGuiApplication::screens())
        geometry = geometry.united(screen->virtualGeometry());
    return geometry;
}

} // namespace

std::optional<QPoint> followAlongXWaylandPointerPosition()
{
#if defined(UOS_AI_LIBRE_HAVE_X11_RECORD) \
    || defined(UOS_AI_LIBRE_HAVE_X11_SHORTCUT)
    if (qEnvironmentVariableIsEmpty("DISPLAY"))
        return std::nullopt;

    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return std::nullopt;

    Window root = DefaultRootWindow(display);
    Window child = 0;
    int rootX = 0;
    int rootY = 0;
    int winX = 0;
    int winY = 0;
    unsigned int mask = 0;
    const bool queried = XQueryPointer(display, root, &root, &child, &rootX,
                                       &rootY, &winX, &winY, &mask)
        != 0;
    const bool reliable = coordinateSpaceMatches(display);
    XCloseDisplay(display);

    if (!queried || !reliable)
        return std::nullopt;

    // XWayland's root starts at 0,0 for the bounding box of all outputs.
    // Translate back if Qt's virtual desktop uses negative coordinates.
    const QRect desktop = virtualDesktopGeometry();
    return QPoint(rootX + desktop.x(), rootY + desktop.y());
#else
    return std::nullopt;
#endif
}

bool followAlongXWaylandCoordinateSpaceReliable()
{
#if defined(UOS_AI_LIBRE_HAVE_X11_RECORD) \
    || defined(UOS_AI_LIBRE_HAVE_X11_SHORTCUT)
    if (qEnvironmentVariableIsEmpty("DISPLAY"))
        return false;
    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return false;
    const bool reliable = coordinateSpaceMatches(display);
    XCloseDisplay(display);
    return reliable;
#else
    return false;
#endif
}
