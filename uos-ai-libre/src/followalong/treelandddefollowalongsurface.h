// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongview.h"

#include <memory>

class TreelandDdePlacement;
class QTimer;

/// Treeland dde-shell overlay backend.
///
/// The Qt surface is an xdg-toplevel, but Treeland's dde-shell protocol adds
/// overlay role, skip flags, keyboard-focus policy and cursor-relative
/// placement. The compositor therefore owns the position, so the toolbar can
/// follow the real pointer even when XWayland's pointer is stale over native
/// Wayland clients. This is the model UOS AI 3.3 uses.
class TreelandDdeFollowAlongSurface final : public FollowAlongSurface
{
    Q_OBJECT

public:
    TreelandDdeFollowAlongSurface(QQmlEngine *engine, FollowAlongEventBus *bus,
                                  QObject *parent = nullptr);
    ~TreelandDdeFollowAlongSurface() override;

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
    void openMoreMenu() override;

protected:
    void windowExposed() override;
    void windowSurfaceCreated() override;

private:
    PresentationCommandResult showSurface(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content, bool updateOnly);
    void wireRenderedFrame();
    void postMappedIfNeeded(quint64 requestId);
    void placeBelowCursor(bool force = false);
    void setKeyboardFocus(bool accepted);
    void schedulePlacement();
    void emitPresented(quint64 requestId);

    std::unique_ptr<TreelandDdePlacement> m_placement;
    QTimer *m_placeTimer = nullptr;
    quint64 m_mappedRequestId = 0;
    bool m_overlayAttached = false;
    bool m_frameWired = false;
    bool m_sizeWired = false;
};
