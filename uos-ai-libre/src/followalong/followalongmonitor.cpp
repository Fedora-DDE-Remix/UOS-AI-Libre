// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongmonitor.h"

#include "x11followalongmonitor.h"

#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
#include "waylandfollowalongmonitor.h"
#endif

#include <QGuiApplication>

FollowAlongMonitor::FollowAlongMonitor(QObject *parent) : QObject(parent) {}

FollowAlongMonitor *FollowAlongMonitor::create(QObject *parent)
{
#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
    if (QGuiApplication::platformName().contains(QStringLiteral("wayland"),
                                                 Qt::CaseInsensitive)) {
        return new WaylandFollowAlongMonitor(parent);
    }
#endif
    return new X11FollowAlongMonitor(parent);
}
