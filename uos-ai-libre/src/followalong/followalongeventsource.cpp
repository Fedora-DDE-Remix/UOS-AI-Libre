// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongeventsource.h"

#include "followalongeventbus.h"
#include "x11followalongeventsource.h"

#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
#include "treelandfollowalongeventsource.h"
#endif

#include <QDateTime>
#include <QGuiApplication>

FollowAlongEventSource::FollowAlongEventSource(FollowAlongEventBus *bus,
                                               QObject *parent)
    : QObject(parent), m_bus(bus)
{
}

FollowAlongEventSource *FollowAlongEventSource::create(
    FollowAlongEventBus *bus, QObject *parent)
{
#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
    if (QGuiApplication::platformName().contains(QStringLiteral("wayland"),
                                                 Qt::CaseInsensitive)) {
        return new TreelandFollowAlongEventSource(bus, parent);
    }
#endif
    return new X11FollowAlongEventSource(bus, parent);
}

void FollowAlongEventSource::post(SelectionEvent event)
{
    if (event.timestampMs <= 0)
        event.timestampMs = QDateTime::currentMSecsSinceEpoch();
    if (m_bus)
        m_bus->postSelectionEvent(event);
}
