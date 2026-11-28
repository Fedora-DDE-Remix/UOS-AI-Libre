// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongmonitor.h"

#include <memory>

class WaylandDataControlManager;
class WaylandDataControlDevice;
class WaylandActiveNotifyManager;
class WaylandActiveNotify;

/// Wayland/Treeland global PRIMARY-selection monitor.
///
/// QClipboard only sees the clipboard while the application is focused, so
/// FollowAlong uses wlr-data-control. Treeland compositors implement it for
/// privileged DDE clients; the toolbar surface itself is handled separately.
class WaylandFollowAlongMonitor final : public FollowAlongMonitor
{
    Q_OBJECT

public:
    explicit WaylandFollowAlongMonitor(QObject *parent = nullptr);
    ~WaylandFollowAlongMonitor() override;

    bool start(QString *error) override;
    void stop() override;
    void resetSelectionState() override;

private:
    void createDevice();
    void createActiveNotifier();

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
};
