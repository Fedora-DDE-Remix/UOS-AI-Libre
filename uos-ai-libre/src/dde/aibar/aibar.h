// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <dock/dappletdock.h>
#include <dock/dockiteminfo.h>

#include <QObject>

namespace uosailibre {

/// Native dde-shell dock applet at dockOrder 26 (between the tray and
/// Show Desktop), matching UOS AI's original org.deepin.ds.dock.aibar slot.
class AiBar final : public DS_NAMESPACE::DAppletDock
{
    Q_OBJECT

public:
    explicit AiBar(QObject *parent = nullptr);

    Q_INVOKABLE void onClickIcon();
    DockItemInfo dockItemInfo() override;
};

} // namespace uosailibre
