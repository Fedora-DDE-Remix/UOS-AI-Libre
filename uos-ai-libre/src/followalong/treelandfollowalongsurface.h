// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongview.h"

class QScreen;

/// Treeland presentation backend.
///
/// It uses wlr-layer-shell through dde-shell's DLayerShellWindow QML binding.
/// The layer role is created by Qt's Wayland shell integration on the first
/// map, so the surface is never an xdg-toplevel. Icon state uses
/// KeyboardInteractivityNone; the expanded toolbar switches to OnDemand only
/// for explicit text/menu interaction.
class TreelandFollowAlongSurface final : public FollowAlongSurface
{
    Q_OBJECT

public:
    TreelandFollowAlongSurface(QQmlEngine *engine, FollowAlongEventBus *bus,
                               QObject *parent = nullptr);
    ~TreelandFollowAlongSurface() override;

    FollowAlongCapabilities capabilities() const override;
    PresentationCommandResult present(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content) override;
    PresentationCommandResult update(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content) override;
    void dismiss() override;
    void requestTextInputFocus() override;
    QRect geometry() const override;
    bool isFocused() const override;
    bool usesInlineMenu() const override { return true; }

protected:
    void windowExposed() override;

private:
    PresentationCommandResult showSurface(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content, bool updateOnly);
    void applyLayerGeometry();
    void wireSizeChanges();
    bool layerRoleEffective() const;
    void confirmLayerRole(quint64 requestId, int attempt);
    void emitPresented(quint64 requestId, bool roleConfirmed);

    quint64 m_mappedRequestId = 0;
    quint64 m_roleRequestId = 0;
    quint64 m_confirmedRoleRequestId = 0;
    quint64 m_presentedRequestId = 0;
    bool m_roleConfirmed = false;
    bool m_sizeWired = false;
};
