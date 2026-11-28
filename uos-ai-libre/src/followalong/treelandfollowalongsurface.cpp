// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "treelandfollowalongsurface.h"

#include "followalongeventbus.h"
#include "followalongxwayland.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include <private/qwaylandwindow_p.h>
#include <private/qwaylandshellsurface_p.h>

Q_LOGGING_CATEGORY(logFollowAlongLayer, "uosai.followalong.layershell")

TreelandFollowAlongSurface::TreelandFollowAlongSurface(
    QQmlEngine *engine, FollowAlongEventBus *bus, QObject *parent)
    : FollowAlongSurface(engine, bus, parent)
{
    m_backendName = QStringLiteral("Treeland layer-shell");
}

TreelandFollowAlongSurface::~TreelandFollowAlongSurface() = default;

FollowAlongCapabilities TreelandFollowAlongSurface::capabilities() const
{
    FollowAlongCapabilities caps;
    caps.globalCursorPosition = followAlongXWaylandCoordinateSpaceReliable();
    caps.releaseGating = true;
    caps.overlayRole = true;
    caps.focusFreeShow = true;
    caps.globalHitTest = caps.globalCursorPosition;
    caps.compositorPlacement = false;
    caps.backendName = m_backendName;
    return caps;
}

PresentationCommandResult TreelandFollowAlongSurface::present(
    quint64 requestId, const FollowAlongPresentation &spec,
    const FollowAlongContent &content)
{
    return showSurface(requestId, spec, content, false);
}

PresentationCommandResult TreelandFollowAlongSurface::update(
    quint64 requestId, const FollowAlongPresentation &spec,
    const FollowAlongContent &content)
{
    return showSurface(requestId, spec, content, true);
}

PresentationCommandResult TreelandFollowAlongSurface::showSurface(
    quint64 requestId, const FollowAlongPresentation &spec,
    const FollowAlongContent &content, bool updateOnly)
{
    if (spec.layer != FollowAlongPresentation::Layer::Overlay
        || !spec.alwaysOnTop || spec.showInTaskSwitcher
        || spec.showInDockPreview || spec.showInMultitaskView) {
        return {false, requestId,
                QStringLiteral("unsupported layer-shell presentation policy")};
    }

    QQuickWindow *window = createWindow(QStringLiteral("FollowAlongLayerWindow"));
    if (!window) {
        return {false, requestId,
                QStringLiteral("wlr-layer-shell surface unavailable")};
    }

    m_lastRequestId = requestId;
    m_backendName = QStringLiteral("Treeland layer-shell");

    std::optional<QPoint> anchor = spec.anchor;
    if (!anchor.has_value())
        anchor = followAlongXWaylandPointerPosition();
    QScreen *screen = nullptr;
    if (anchor.has_value()) {
        m_anchor = *anchor;
        m_hasAnchor = true;
        screen = screenForAnchor(m_anchor);
    } else {
        // Layer-shell has no compositor auto-placement relative to the
        // pointer. Without XWayland we place the icon in a stable, visible
        // spot on the primary output instead of silently creating an
        // xdg-toplevel.
        screen = QGuiApplication::primaryScreen();
        m_hasAnchor = false;
        if (screen) {
            const QRect area = screen->availableGeometry();
            m_anchor = QPoint(area.center().x(), area.top() + area.height() / 3);
        }
    }
    if (screen)
        window->setScreen(screen);

    applyContent(content);
    window->setProperty("keyboardInteractive",
                        content.expanded
                            || spec.focusPolicy
                                == FollowAlongPresentation::FocusPolicy::OnExplicitRequest);
    if (window->width() <= 0 || window->height() <= 0) {
        const QSize fallback = spec.size.isValid()
            ? spec.size
            : QSize(content.expanded ? 425 : 24, content.expanded ? 40 : 24);
        window->resize(clampedSize(m_anchor, fallback));
    }
    wireSizeChanges();
    applyLayerGeometry();

    qInfo() << "FollowAlong: layer-shell" << (updateOnly ? "update" : "present")
            << "at" << m_anchor << "exact" << m_hasAnchor << "size"
            << window->size() << "expanded" << content.expanded;

    window->show();

    FollowAlongPresentationState accepted;
    accepted.accepted = true;
    accepted.overlayEffective = m_roleConfirmed;
    accepted.focused = isFocused();
    accepted.geometry = geometry();
    postPresentation(PresentationEventType::Accepted, requestId, accepted);

    if (m_roleConfirmed) {
        confirmLayerRole(requestId, 0);
    } else {
        m_roleRequestId = requestId;
        confirmLayerRole(requestId, 0);
    }

    return {true, requestId, QString()};
}

void TreelandFollowAlongSurface::confirmLayerRole(quint64 requestId,
                                                  int attempt)
{
    if (!m_window || m_lastRequestId != requestId)
        return;

    if (layerRoleEffective()) {
        m_roleConfirmed = true;
        m_confirmedRoleRequestId = requestId;
        qInfo() << "FollowAlong: layer-shell role confirmed for request"
                << requestId << "focused" << isFocused();
        emitPresented(requestId, true);
        return;
    }

    if (attempt >= 20) {
        FollowAlongPresentationState state;
        state.accepted = true;
        state.focused = isFocused();
        state.geometry = geometry();
        const QString error =
            QStringLiteral("wlr-layer-shell role was not applied; "
                           "refusing to expose a normal xdg-toplevel");
        qCWarning(logFollowAlongLayer).noquote() << error;
        postPresentation(PresentationEventType::Failed, requestId, state,
                         error);
        return;
    }

    QTimer::singleShot(50, this, [this, requestId, attempt]() {
        confirmLayerRole(requestId, attempt + 1);
    });
}

void TreelandFollowAlongSurface::emitPresented(quint64 requestId,
                                                bool roleConfirmed)
{
    if (!m_window || m_lastRequestId != requestId)
        return;
    if (m_presentedRequestId == requestId)
        return;
    m_presentedRequestId = requestId;

    FollowAlongPresentationState state;
    state.accepted = true;
    state.overlayEffective = roleConfirmed;
    state.focused = isFocused();
    state.inTaskSwitcher = false;
    state.geometry = geometry();

    if (!roleConfirmed) {
        postPresentation(PresentationEventType::Failed, requestId, state,
                         QStringLiteral("layer-shell role not effective"));
        return;
    }

    postPresentation(PresentationEventType::RoleConfirmed, requestId, state);

    if (m_window->isExposed()) {
        if (m_mappedRequestId != requestId) {
            m_mappedRequestId = requestId;
            postPresentation(PresentationEventType::Mapped, requestId, state);
        }
    }

    // Icon state must not hold keyboard focus. If the compositor granted it
    // (for example a stale OnDemand request), report failure instead of
    // pretending focus-free presentation succeeded.
    if (!m_content.expanded && state.focused) {
        postPresentation(PresentationEventType::Failed, requestId, state,
                         QStringLiteral("layer-shell surface accepted "
                                        "keyboard focus in icon state"));
        return;
    }

    postPresentation(PresentationEventType::FocusChanged, requestId, state);
    postPresentation(PresentationEventType::GeometryChanged, requestId, state);
}

bool TreelandFollowAlongSurface::layerRoleEffective() const
{
    if (!m_window || !m_window->handle())
        return false;
    auto *waylandWindow = dynamic_cast<QtWaylandClient::QWaylandWindow *>(
        m_window->handle());
    if (!waylandWindow)
        return false;
    auto *shellSurface = waylandWindow->shellSurface();
    if (!shellSurface)
        return false;
    return shellSurface->inherits("ds::QWaylandLayerShellSurface")
        || shellSurface->inherits("LayerShellQt::QWaylandLayerSurface");
}

void TreelandFollowAlongSurface::applyLayerGeometry()
{
    if (!m_window || !m_window->screen())
        return;

    QSize size = m_window->size();
    if (!size.isValid() || size.isEmpty())
        return;
    const QSize bounded = clampedSize(m_anchor, size);
    if (bounded != size) {
        // QML already clamps with maxContentWidth. This is a defensive
        // fallback for a stale content width.
        size = bounded;
    }
    QPoint globalPosition =
        positionForSize(m_anchor, QSize(size.width(), 40));
    if (m_window->property("menuAbove").toBool())
        globalPosition.setY(
            globalPosition.y() - qMax(0, size.height() - 40));

    // Keep the whole surface, including an in-window menu, on the output.
    const QRect area = availableRect(m_anchor);
    const QRect content = area.adjusted(6, 6, -6, -6);
    const int maxX = qMax(content.left(), content.right() - size.width() + 1);
    const int maxY = qMax(content.top(), content.bottom() - size.height() + 1);
    globalPosition.setX(qBound(content.left(), globalPosition.x(), maxX));
    globalPosition.setY(qBound(content.top(), globalPosition.y(), maxY));

    const QRect screenGeometry = m_window->screen()->geometry();
    const int surfaceX = globalPosition.x() - screenGeometry.x();
    const int surfaceY = globalPosition.y() - screenGeometry.y();
    m_lastGeometry = QRect(globalPosition, size);

    m_window->setProperty("surfaceX", surfaceX);
    m_window->setProperty("surfaceY", surfaceY);
    // Keep QWindow x/y in sync for Qt input coordinate mapping and popup
    // placement. The layer-shell margins remain the authoritative position.
    m_window->setX(globalPosition.x());
    m_window->setY(globalPosition.y());
}

void TreelandFollowAlongSurface::wireSizeChanges()
{
    if (!m_window || m_sizeWired)
        return;
    m_sizeWired = true;
    const auto onSizeChanged = [this]() {
        applyLayerGeometry();
        if (!m_window || m_lastRequestId == 0)
            return;
        FollowAlongPresentationState state;
        state.accepted = true;
        state.overlayEffective = m_roleConfirmed;
        state.focused = isFocused();
        state.geometry = geometry();
        postPresentation(PresentationEventType::GeometryChanged,
                         m_lastRequestId, state);
    };
    connect(m_window, &QQuickWindow::widthChanged, this, onSizeChanged);
    connect(m_window, &QQuickWindow::heightChanged, this, onSizeChanged);
}

void TreelandFollowAlongSurface::windowExposed()
{
    if (!m_window || m_lastRequestId == 0)
        return;
    if (!m_roleConfirmed) {
        // The first expose is also valid proof that the layer surface was
        // configured. Recheck the real shell-surface type here instead of
        // trusting the client-side attach call.
        if (layerRoleEffective()) {
            m_roleConfirmed = true;
            m_confirmedRoleRequestId = m_lastRequestId;
            emitPresented(m_lastRequestId, true);
        }
        return;
    }
    if (m_mappedRequestId == m_lastRequestId)
        return;
    m_mappedRequestId = m_lastRequestId;
    FollowAlongPresentationState state;
    state.accepted = true;
    state.overlayEffective = true;
    state.focused = isFocused();
    state.geometry = geometry();
    postPresentation(PresentationEventType::Mapped, m_lastRequestId, state);
}

void TreelandFollowAlongSurface::dismiss()
{
    if (!m_window)
        return;
    m_window->setProperty("keyboardInteractive", false);
    releaseSwapchain();
    m_window->hide();
    FollowAlongPresentationState state;
    state.accepted = true;
    state.overlayEffective = m_roleConfirmed;
    state.focused = false;
    state.geometry = geometry();
    postPresentation(PresentationEventType::Dismissed, m_lastRequestId, state);
}

void TreelandFollowAlongSurface::requestTextInputFocus()
{
    if (!m_window)
        return;
    qInfo() << "FollowAlong: enabling text input focus";
    // The surface is already in the layer-shell overlay and the user's click
    // grants OnDemand keyboard focus. requestActivate() on a layer surface is
    // rejected by Treeland ("surface has no ActiveCapability") and is not
    // needed for the click-to-type path.
    m_window->setProperty("keyboardInteractive", true);
}

QRect TreelandFollowAlongSurface::geometry() const
{
    if (!m_lastGeometry.isValid() && m_window)
        return QRect(m_window->position(), m_window->size());
    return m_lastGeometry;
}

bool TreelandFollowAlongSurface::isFocused() const
{
    return m_window && m_window->isActive();
}
