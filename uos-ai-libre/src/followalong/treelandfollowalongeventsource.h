// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongeventsource.h"

#include <memory>

class WaylandDataControlManager;
class WaylandDataControlDevice;
class WaylandActiveNotifyManager;
class WaylandActiveNotify;

/// Treeland selection source.
///
/// wlr-data-control provides the global PRIMARY selection and
/// treeland-active-notify provides the left-button release gate. The two
/// channels are asynchronous; this class repairs their ordering and attaches
/// the XWayland pointer anchor when the coordinate spaces match.
class TreelandFollowAlongEventSource final : public FollowAlongEventSource
{
    Q_OBJECT

public:
    explicit TreelandFollowAlongEventSource(FollowAlongEventBus *bus,
                                            QObject *parent = nullptr);
    ~TreelandFollowAlongEventSource() override;

    bool start(QString *error) override;
    void stop() override;
    void resetSelectionState() override;
    FollowAlongCapabilities capabilities() const override;

private:
    void createDevice();
    void createActiveNotifier();
    void publishPending(const QPoint *anchor);
    void postPhase(SelectionPhase phase, const QString &text,
                   const std::optional<QPoint> &anchor, bool fromDrag,
                   qint64 timestampMs);

    std::unique_ptr<WaylandDataControlManager> m_manager;
    std::unique_ptr<WaylandDataControlDevice> m_device;
    std::unique_ptr<WaylandActiveNotifyManager> m_activeManager;
    std::unique_ptr<WaylandActiveNotify> m_activeNotify;
    QString m_pendingText;
    qint64 m_pendingAtMs = 0;
    bool m_mouseActive = false;
    bool m_mouseGesture = false;
    bool m_hasMouseActivity = false;
    bool m_active = false;
    quint64 m_sequence = 0;
};
