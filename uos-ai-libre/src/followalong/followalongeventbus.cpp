// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongeventbus.h"

#include <QDateTime>
#include <QMetaObject>

namespace {
constexpr qint64 kOwnSurfaceGraceMs = 2500;
}

FollowAlongEventBus::FollowAlongEventBus(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<SelectionEvent>("SelectionEvent");
    qRegisterMetaType<SelectionPhase>("SelectionPhase");
    qRegisterMetaType<FollowAlongPresentation>("FollowAlongPresentation");
    qRegisterMetaType<FollowAlongPresentationState>("FollowAlongPresentationState");
    qRegisterMetaType<PresentationCommandResult>("PresentationCommandResult");
    qRegisterMetaType<PresentationEventType>("PresentationEventType");
    qRegisterMetaType<PresentationEvent>("PresentationEvent");
}

qint64 FollowAlongEventBus::lastOwnSurfaceInteractionMs() const
{
    return m_lastOwnSurfaceInteractionMs;
}

void FollowAlongEventBus::noteOwnSurfaceInteraction(qint64 timestampMs)
{
    m_lastOwnSurfaceInteractionMs = timestampMs > 0
        ? timestampMs
        : QDateTime::currentMSecsSinceEpoch();
}

void FollowAlongEventBus::postSelectionEvent(SelectionEvent event)
{
    if (event.timestampMs <= 0)
        event.timestampMs = QDateTime::currentMSecsSinceEpoch();
    if (event.phase == SelectionPhase::Cleared
        && m_lastOwnSurfaceInteractionMs > 0
        && event.timestampMs - m_lastOwnSurfaceInteractionMs <= kOwnSurfaceGraceMs) {
        event.causedByOwnSurface = true;
    }

    QMetaObject::invokeMethod(
        this, [this, event]() { dispatchSelection(event); },
        Qt::QueuedConnection);
}

void FollowAlongEventBus::postPresentationEvent(PresentationEvent event)
{
    QMetaObject::invokeMethod(
        this, [this, event]() { dispatchPresentation(event); },
        Qt::QueuedConnection);
}

void FollowAlongEventBus::dispatchSelection(const SelectionEvent &event)
{
    Q_EMIT selectionEvent(event);
}

void FollowAlongEventBus::dispatchPresentation(const PresentationEvent &event)
{
    Q_EMIT presentationEvent(event);
}
