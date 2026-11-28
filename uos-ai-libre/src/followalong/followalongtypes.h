// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaType>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVariantList>

#include <optional>

/// Semantic selection events consumed by FollowAlongController.
///
/// The event sources normalise X11 (PRIMARY + XRecord) and Treeland
/// (wlr-data-control + treeland-active-notify) into this small vocabulary.
/// Coordinates are optional because Wayland does not expose a global pointer
/// to ordinary clients.
enum class SelectionPhase {
    GestureBegan,
    TextPublished,
    GestureEnded,
    Cleared,
};

struct SelectionEvent {
    SelectionPhase phase = SelectionPhase::Cleared;
    QString text;
    std::optional<QPoint> globalAnchor;
    Qt::MouseButton button = Qt::NoButton;
    bool fromDrag = false;
    quint64 sequence = 0;
    qint64 timestampMs = 0;
    bool causedByOwnSurface = false;
};

/// Capabilities are properties of the platform backend, not of the user
/// intent. The controller uses them to pick a legal degradation instead of
/// branching on X11/Wayland by name.
struct FollowAlongCapabilities {
    bool globalCursorPosition = false;
    bool releaseGating = false;
    bool overlayRole = false;
    bool focusFreeShow = false;
    bool globalHitTest = false;
    bool compositorPlacement = false;
    QString backendName;
};

/// Content is deliberately kept out of the presentation contract: the
/// execution layer owns how the surface is shown, while the service owns what
/// text/actions are shown. UI-to-aish routing stays on the service.
struct FollowAlongContent {
    QString selectedText;
    QVariantList actions;
    QVariantList moreActions;
    bool expanded = false;
};

/// A semantic presentation request. The controller never names X11 atoms,
/// dde-shell roles or layer-shell requests; the backend translates this into
/// concrete platform operations and reports whether it actually succeeded.
struct FollowAlongPresentation {
    enum class Layer { Overlay };
    enum class FocusPolicy { Never, OnExplicitRequest };
    enum class AnchorMode { GlobalPoint, Cursor, None };

    Layer layer = Layer::Overlay;
    bool alwaysOnTop = true;
    bool showInTaskSwitcher = false;
    bool showInDockPreview = false;
    bool showInMultitaskView = false;
    FocusPolicy focusPolicy = FocusPolicy::Never;
    AnchorMode anchorMode = AnchorMode::None;
    std::optional<QPoint> anchor;
    QSize size;
    bool clampToOutput = true;
};

struct FollowAlongPresentationState {
    bool accepted = false;
    bool overlayEffective = false;
    bool focused = false;
    bool inTaskSwitcher = false;
    std::optional<QRect> geometry;
    QString error;
    QString backendName;
};

/// Result of the initial command call. `accepted` only means the request was
/// valid and handed to the backend; it is not proof that the compositor
/// mapped the surface with the requested role.
struct PresentationCommandResult {
    bool accepted = false;
    quint64 requestId = 0;
    QString error;
};

enum class PresentationEventType {
    Accepted,
    Rejected,
    RoleConfirmed,
    Mapped,
    FocusChanged,
    GeometryChanged,
    Dismissed,
    Failed,
};

struct PresentationEvent {
    PresentationEventType type = PresentationEventType::Failed;
    quint64 requestId = 0;
    FollowAlongPresentationState state;
    bool causedByOwnSurface = false;
    QString error;
};

Q_DECLARE_METATYPE(SelectionEvent)
Q_DECLARE_METATYPE(SelectionPhase)
Q_DECLARE_METATYPE(FollowAlongPresentation)
Q_DECLARE_METATYPE(FollowAlongPresentationState)
Q_DECLARE_METATYPE(PresentationCommandResult)
Q_DECLARE_METATYPE(PresentationEventType)
Q_DECLARE_METATYPE(PresentationEvent)

inline QString selectionPhaseName(SelectionPhase phase)
{
    switch (phase) {
    case SelectionPhase::GestureBegan:
        return QStringLiteral("GestureBegan");
    case SelectionPhase::TextPublished:
        return QStringLiteral("TextPublished");
    case SelectionPhase::GestureEnded:
        return QStringLiteral("GestureEnded");
    case SelectionPhase::Cleared:
        return QStringLiteral("Cleared");
    }
    return QStringLiteral("Unknown");
}

inline QString presentationEventName(PresentationEventType type)
{
    switch (type) {
    case PresentationEventType::Accepted:
        return QStringLiteral("Accepted");
    case PresentationEventType::Rejected:
        return QStringLiteral("Rejected");
    case PresentationEventType::RoleConfirmed:
        return QStringLiteral("RoleConfirmed");
    case PresentationEventType::Mapped:
        return QStringLiteral("Mapped");
    case PresentationEventType::FocusChanged:
        return QStringLiteral("FocusChanged");
    case PresentationEventType::GeometryChanged:
        return QStringLiteral("GeometryChanged");
    case PresentationEventType::Dismissed:
        return QStringLiteral("Dismissed");
    case PresentationEventType::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}
