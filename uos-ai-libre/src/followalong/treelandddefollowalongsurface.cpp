// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "treelandddefollowalongsurface.h"

#include "followalongeventbus.h"

#include <QDebug>
#include <QGuiApplication>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>
#include <QWindow>
#include <QWaylandClientExtension>

#include <qwayland-treeland-dde-shell-v1.h>

#include <private/qwaylandwindow_p.h>

#include <functional>

Q_LOGGING_CATEGORY(logFollowAlongDde, "uosai.followalong.ddeshell")

class TreelandDdePlacement
{
public:
    class Manager final
        : public QWaylandClientExtensionTemplate<Manager>
        , public QtWayland::treeland_dde_shell_manager_v1
    {
    public:
        Manager()
            : QWaylandClientExtensionTemplate<Manager>(1)
        {
            initialize();
        }
    };

    class Surface final : public QtWayland::treeland_dde_shell_surface_v1
    {
    public:
        explicit Surface(struct ::treeland_dde_shell_surface_v1 *id)
            : QtWayland::treeland_dde_shell_surface_v1(id)
        {
        }

        ~Surface() override { destroy(); }
    };

    void setSurfaceDestroyedCallback(std::function<void()> callback)
    {
        m_surfaceDestroyed = std::move(callback);
    }

    bool isAttached() const { return m_surface != nullptr; }

    bool ensureAttached(QWindow *window)
    {
        if (!window)
            return false;
        if (!m_manager)
            m_manager = std::make_unique<Manager>();
        if (!m_manager->isActive())
            return false;

        auto *waylandWindow = dynamic_cast<QtWaylandClient::QWaylandWindow *>(
            window->handle());
        if (!waylandWindow)
            return false;

        if (m_connectedWindow != waylandWindow) {
            m_connectedWindow = waylandWindow;
            // QWaylandWindow emits surfaceCreated from initializeWlSurface(),
            // i.e. before initWindow() creates the xdg_toplevel. Attaching the
            // dde-shell metadata synchronously here guarantees Treeland sees it
            // when it creates the SurfaceWrapper. This also runs on every
            // re-show, after QWaylandWindow::reset() destroyed the old surface.
            QObject::connect(waylandWindow,
                             &QtWaylandClient::QWaylandWindow::surfaceCreated,
                             window, [this]() { attachCurrentSurface(); });
            QObject::connect(waylandWindow,
                             &QtWaylandClient::QWaylandWindow::surfaceDestroyed,
                             window, [this]() {
                                 m_surface.reset();
                                 if (m_surfaceDestroyed)
                                     m_surfaceDestroyed();
                             });
        }

        if (waylandWindow->surface())
            attachCurrentSurface();
        return m_surface != nullptr;
    }

    void placeBelowCursor(bool force)
    {
        m_yOffset = 12;
        if (!m_surface)
            return;
        // The compositor only re-runs placeUnderCursor when the y offset
        // changes. Toggle through a different value to force a fresh placement
        // after the toolbar/menu changes size.
        if (force)
            m_surface->set_auto_placement(13);
        m_surface->set_auto_placement(m_yOffset);
    }

    void setKeyboardFocus(bool accepted)
    {
        m_acceptFocus = accepted;
        if (m_surface)
            m_surface->set_accept_keyboard_focus(accepted ? 1 : 0);
    }

private:
    void attachCurrentSurface()
    {
        if (m_surface)
            return;
        auto *waylandWindow = m_connectedWindow.data();
        if (!waylandWindow || !waylandWindow->surface() || !m_manager
            || !m_manager->isActive())
            return;

        auto *raw = m_manager->get_shell_surface(waylandWindow->surface());
        if (!raw)
            return;
        m_surface = std::make_unique<Surface>(raw);
        m_surface->set_role(
            QtWayland::treeland_dde_shell_surface_v1::role_overlay);
        m_surface->set_skip_switcher(1);
        m_surface->set_skip_dock_preview(1);
        m_surface->set_skip_muti_task_view(1);
        m_surface->set_accept_keyboard_focus(m_acceptFocus ? 1 : 0);
        m_surface->set_auto_placement(m_yOffset);
        qInfo() << "FollowAlong: dde-shell overlay attached";
    }

    std::unique_ptr<Manager> m_manager;
    std::unique_ptr<Surface> m_surface;
    std::function<void()> m_surfaceDestroyed;
    QPointer<QtWaylandClient::QWaylandWindow> m_connectedWindow;
    quint32 m_yOffset = 12;
    bool m_acceptFocus = false;
};

TreelandDdeFollowAlongSurface::TreelandDdeFollowAlongSurface(
    QQmlEngine *engine, FollowAlongEventBus *bus, QObject *parent)
    : FollowAlongSurface(engine, bus, parent)
    , m_placement(std::make_unique<TreelandDdePlacement>())
{
    m_backendName = QStringLiteral("Treeland dde-shell overlay");
    m_placement->setSurfaceDestroyedCallback([this]() {
        m_overlayAttached = false;
        qInfo() << "FollowAlong: dde-shell surface destroyed; will reattach";
    });
}

TreelandDdeFollowAlongSurface::~TreelandDdeFollowAlongSurface() = default;

FollowAlongCapabilities TreelandDdeFollowAlongSurface::capabilities() const
{
    FollowAlongCapabilities caps;
    caps.globalCursorPosition = true; // compositor auto placement
    caps.releaseGating = true;
    caps.overlayRole = true;
    caps.focusFreeShow = true;
    caps.globalHitTest = false; // handled by Enter/Leave + local press
    caps.compositorPlacement = true;
    caps.backendName = m_backendName;
    return caps;
}

PresentationCommandResult TreelandDdeFollowAlongSurface::present(
    quint64 requestId, const FollowAlongPresentation &spec,
    const FollowAlongContent &content)
{
    return showSurface(requestId, spec, content, false);
}

PresentationCommandResult TreelandDdeFollowAlongSurface::update(
    quint64 requestId, const FollowAlongPresentation &spec,
    const FollowAlongContent &content)
{
    return showSurface(requestId, spec, content, true);
}

PresentationCommandResult TreelandDdeFollowAlongSurface::showSurface(
    quint64 requestId, const FollowAlongPresentation &spec,
    const FollowAlongContent &content, bool updateOnly)
{
    Q_UNUSED(updateOnly);
    if (spec.layer != FollowAlongPresentation::Layer::Overlay
        || !spec.alwaysOnTop || spec.showInTaskSwitcher) {
        return {false, requestId,
                QStringLiteral("unsupported dde-shell presentation policy")};
    }

    QQuickWindow *window = createWindow(QStringLiteral("FollowAlongDdeWindow"));
    if (!window) {
        return {false, requestId,
                QStringLiteral("dde-shell FollowAlong window unavailable")};
    }

    m_lastRequestId = requestId;
    m_swapchainReleased = false;
    m_backendName = QStringLiteral("Treeland dde-shell overlay");
    applyContent(content);

    // QWaylandWindow::setVisible(true) destroys and recreates the wl_surface
    // on every show. Attach the dde-shell role from the QWaylandWindow
    // surfaceCreated signal, which is emitted before the xdg-toplevel is
    // created, then show the already-connected window.
    const bool wasVisible = window->isVisible();
    window->create();
    m_placement->ensureAttached(window);
    setKeyboardFocus(content.expanded);
    window->show();
    if (!m_placement->isAttached()) {
        window->hide();
        return {false, requestId,
                QStringLiteral("treeland_dde_shell_manager_v1 unavailable")};
    }
    m_overlayAttached = true;
    // Place only when the surface was not already visible. Re-running auto
    // placement from an icon click uses the *current* cursor (now over the
    // icon/pill), which made the toolbar jump down-right on every expansion.
    // Keep the original top-left for icon->expanded/menu transitions and let
    // the compositor's size-change layout clamp the surface to the output.
    if (!updateOnly || !wasVisible)
        placeBelowCursor();
    wireRenderedFrame();

    FollowAlongPresentationState accepted;
    accepted.accepted = true;
    accepted.overlayEffective = true;
    accepted.focused = isFocused();
    accepted.geometry = geometry();
    postPresentation(PresentationEventType::Accepted, requestId, accepted);

    emitPresented(requestId);
    return {true, requestId, QString()};
}


void TreelandDdeFollowAlongSurface::placeBelowCursor(bool force)
{
    m_placement->placeBelowCursor(force);
}


void TreelandDdeFollowAlongSurface::setKeyboardFocus(bool accepted)
{
    m_placement->setKeyboardFocus(accepted);
}

void TreelandDdeFollowAlongSurface::emitPresented(quint64 requestId)
{
    if (!m_window || m_lastRequestId != requestId || !m_overlayAttached)
        return;

    FollowAlongPresentationState state;
    state.accepted = true;
    state.overlayEffective = true;
    state.focused = isFocused();
    state.inTaskSwitcher = false;
    state.geometry = geometry();

    postPresentation(PresentationEventType::RoleConfirmed, requestId, state);
    postMappedIfNeeded(requestId);
    QTimer::singleShot(300, this, [this, requestId]() {
        if (m_window && m_lastRequestId == requestId)
            postMappedIfNeeded(requestId);
    });

    if (!m_content.expanded && state.focused) {
        postPresentation(PresentationEventType::Failed, requestId, state,
                         QStringLiteral("dde-shell overlay accepted keyboard "
                                        "focus in icon state"));
        return;
    }

    postPresentation(PresentationEventType::FocusChanged, requestId, state);
    postPresentation(PresentationEventType::GeometryChanged, requestId, state);
}

void TreelandDdeFollowAlongSurface::postMappedIfNeeded(quint64 requestId)
{
    if (!m_window || requestId == 0 || m_lastRequestId != requestId)
        return;
    if (m_mappedRequestId == requestId)
        return;
    if (!m_window->isVisible())
        return;
    m_mappedRequestId = requestId;
    FollowAlongPresentationState state;
    state.accepted = true;
    state.overlayEffective = true;
    state.focused = isFocused();
    state.inTaskSwitcher = false;
    state.geometry = geometry();
    postPresentation(PresentationEventType::Mapped, requestId, state);
}

void TreelandDdeFollowAlongSurface::wireRenderedFrame()
{
    if (!m_window || m_frameWired)
        return;
    m_frameWired = true;
    connect(m_window, &QQuickWindow::frameSwapped, this, [this]() {
        postMappedIfNeeded(m_lastRequestId);
    });
}

void TreelandDdeFollowAlongSurface::windowExposed()
{
    if (m_window && m_lastRequestId != 0)
        postMappedIfNeeded(m_lastRequestId);
}

void TreelandDdeFollowAlongSurface::windowSurfaceCreated()
{
    // QWaylandWindow emits SurfaceCreated when the wl_surface is created,
    // before Treeland processes the xdg-toplevel add. Attach here so the
    // server sees the dde-shell overlay metadata while creating the wrapper.
    if (m_window && !m_overlayAttached)
        m_overlayAttached = m_placement->ensureAttached(m_window);
}

void TreelandDdeFollowAlongSurface::dismiss()
{
    if (!m_window)
        return;
    setKeyboardFocus(false);
    releaseSwapchain();
    m_window->hide();
    FollowAlongPresentationState state;
    state.accepted = true;
    state.overlayEffective = true;
    state.focused = false;
    state.geometry = geometry();
    postPresentation(PresentationEventType::Dismissed, m_lastRequestId, state);
}

void TreelandDdeFollowAlongSurface::openMoreMenu()
{
    if (!m_window || !m_window->isVisible())
        return;
    // With compositor auto placement the whole surface stays below the
    // cursor; draw the menu below the pill and let Treeland clamp it.
    m_window->setProperty("menuAbove", false);
    QMetaObject::invokeMethod(m_window, "openMoreMenu");
}

void TreelandDdeFollowAlongSurface::requestTextInputFocus()
{
    if (!m_window)
        return;
    setKeyboardFocus(true);
    m_window->requestActivate();
}

QRect TreelandDdeFollowAlongSurface::geometry() const
{
    return m_window ? m_window->geometry() : QRect();
}

bool TreelandDdeFollowAlongSurface::isFocused() const
{
    return m_window && m_window->isActive();
}
