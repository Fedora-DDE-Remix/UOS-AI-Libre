// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongtypes.h"

#include <QObject>

/// Single asynchronous entry point for all FollowAlong events.
///
/// Event sources and presentation backends post here; the controller only
/// subscribes to the bus. Delivery is queued so a backend can never re-enter
/// the controller from inside present()/dismiss().
class FollowAlongEventBus final : public QObject
{
    Q_OBJECT

public:
    explicit FollowAlongEventBus(QObject *parent = nullptr);

    void postSelectionEvent(SelectionEvent event);
    void postPresentationEvent(PresentationEvent event);

    /// Called by the presentation backend when the user pressed or focused
    /// our own surface. A PRIMARY clear caused inside this window is marked
    /// causedByOwnSurface so it cannot dismiss the toolbar.
    void noteOwnSurfaceInteraction(qint64 timestampMs = 0);
    qint64 lastOwnSurfaceInteractionMs() const;

Q_SIGNALS:
    void selectionEvent(const SelectionEvent &event);
    void presentationEvent(const PresentationEvent &event);

private:
    void dispatchSelection(const SelectionEvent &event);
    void dispatchPresentation(const PresentationEvent &event);

    qint64 m_lastOwnSurfaceInteractionMs = 0;
};
