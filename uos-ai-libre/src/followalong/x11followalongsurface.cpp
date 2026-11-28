// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "x11followalongsurface.h"

#include "followalongeventbus.h"

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#if defined(UOS_AI_LIBRE_HAVE_X11_RECORD) \
    || defined(UOS_AI_LIBRE_HAVE_X11_SHORTCUT)
#include <X11/Xlib.h>
// Xlib's unprefixed macros collide with Qt's enum names.
#undef None
#undef Bool
#undef Status
#undef CursorShape
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef Expose
#endif

X11FollowAlongSurface::X11FollowAlongSurface(QQmlEngine *engine,
                                             FollowAlongEventBus *bus,
                                             QObject *parent)
    : FollowAlongSurface(engine, bus, parent)
{
    m_backendName = QStringLiteral("X11 tool surface");
}

X11FollowAlongSurface::~X11FollowAlongSurface() = default;

FollowAlongCapabilities X11FollowAlongSurface::capabilities() const
{
    FollowAlongCapabilities caps;
    caps.globalCursorPosition = true;
    caps.releaseGating = true;
    caps.overlayRole = true;
    caps.focusFreeShow = true;
    caps.globalHitTest = true;
    caps.compositorPlacement = false;
    caps.backendName = m_backendName;
    return caps;
}

PresentationCommandResult X11FollowAlongSurface::present(
    quint64 requestId, const FollowAlongPresentation &spec,
    const FollowAlongContent &content)
{
    return showSurface(requestId, spec, content, false);
}

PresentationCommandResult X11FollowAlongSurface::update(
    quint64 requestId, const FollowAlongPresentation &spec,
    const FollowAlongContent &content)
{
    return showSurface(requestId, spec, content, true);
}

PresentationCommandResult X11FollowAlongSurface::showSurface(
    quint64 requestId, const FollowAlongPresentation &spec,
    const FollowAlongContent &content, bool updateOnly)
{
    if (spec.layer != FollowAlongPresentation::Layer::Overlay
        || !spec.alwaysOnTop || spec.showInTaskSwitcher) {
        return {false, requestId,
                QStringLiteral("unsupported X11 presentation policy")};
    }

    QQuickWindow *window = createWindow(QStringLiteral("FollowAlongX11Window"));
    if (!window) {
        return {false, requestId,
                QStringLiteral("failed to create X11 FollowAlong window")};
    }

    m_lastRequestId = requestId;
    // A previous hide released the Vulkan swapchain while the native surface
    // was still alive. Qt Quick creates a fresh swapchain when this window is
    // shown again, so the next hide must release it again.
    m_swapchainReleased = false;
    m_backendName = QStringLiteral("X11 tool surface");
    m_anchor = spec.anchor.value_or(QCursor::pos());
    m_hasAnchor = true;
    applyContent(content);
    window->setScreen(screenForAnchor(m_anchor));
    wireSizeChanges();

    if (!updateOnly)
        rememberPreviousFocus();

    // The QML window already has Tool/Frameless/StaysOnTop/NoFocus flags.
    // Position and size are applied together so an X11 WM cannot constrain a
    // resize against the old screen edge.
    if (window->width() <= 0 || window->height() <= 0) {
        const QSize fallback = spec.size.isValid()
            ? spec.size
            : QSize(content.expanded ? 425 : 24, content.expanded ? 40 : 24);
        window->resize(clampedSize(m_anchor, fallback));
    }
    applyGeometry();
    window->show();
    window->raise();

    const quint64 mapped = m_mappedRequestId;
    QTimer::singleShot(0, this,
                       [this, requestId, updateOnly, mapped]() {
        if (!m_window || m_lastRequestId != requestId)
            return;

        FollowAlongPresentationState state;
        state.accepted = true;
        // X11 has no overlay role. The contract is satisfied by a tool window
        // that stays on top and explicitly refuses keyboard focus on show.
        const Qt::WindowFlags flags = m_window->flags();
        state.overlayEffective =
            flags.testFlag(Qt::WindowStaysOnTopHint)
            && flags.testFlag(Qt::WindowDoesNotAcceptFocus)
            && flags.testFlag(Qt::FramelessWindowHint);
        state.inTaskSwitcher = false;
        state.geometry = geometry();
        if (!state.overlayEffective) {
            const QString error =
                QStringLiteral("X11 overlay flags were not applied");
            qWarning().noquote() << "FollowAlong:" << error;
            postPresentation(PresentationEventType::Failed, requestId, state,
                             error);
            return;
        }

        if (!updateOnly)
            restorePreviousFocusIfNeeded();
        state.focused = isFocused();

        postPresentation(PresentationEventType::Accepted, requestId, state);
        postPresentation(PresentationEventType::RoleConfirmed, requestId,
                         state);
        if (mapped != requestId) {
            m_mappedRequestId = requestId;
            postPresentation(PresentationEventType::Mapped, requestId, state);
        }
        postPresentation(PresentationEventType::FocusChanged, requestId,
                         state);
        postPresentation(PresentationEventType::GeometryChanged, requestId,
                         state);
    });

    return {true, requestId, QString()};
}

void X11FollowAlongSurface::wireSizeChanges()
{
    if (!m_window || m_sizeWired)
        return;
    m_sizeWired = true;
    connect(m_window, &QQuickWindow::widthChanged, this,
            [this]() {
                applyGeometry();
                if (!m_window || m_lastRequestId == 0)
                    return;
                FollowAlongPresentationState state;
                state.accepted = true;
                state.overlayEffective = true;
                state.focused = isFocused();
                state.geometry = geometry();
                postPresentation(PresentationEventType::GeometryChanged,
                                 m_lastRequestId, state);
            });
    connect(m_window, &QQuickWindow::heightChanged, this,
            [this]() {
                applyGeometry();
                if (!m_window || m_lastRequestId == 0)
                    return;
                FollowAlongPresentationState state;
                state.accepted = true;
                state.overlayEffective = true;
                state.focused = isFocused();
                state.geometry = geometry();
                postPresentation(PresentationEventType::GeometryChanged,
                                 m_lastRequestId, state);
            });
}

void X11FollowAlongSurface::applyGeometry()
{
    if (!m_window)
        return;
    QSize size = m_window->size();
    if (!size.isValid() || size.isEmpty())
        return;
    const QSize bounded = clampedSize(m_anchor, size);
    QPoint position = positionForSize(m_anchor, QSize(bounded.width(), 40));
    if (m_window->property("menuAbove").toBool())
        position.setY(position.y() - qMax(0, bounded.height() - 40));

    // Keep the whole surface, including an in-window menu, on the output.
    const QRect area = availableRect(m_anchor);
    const QRect content = area.adjusted(6, 6, -6, -6);
    const int maxX = qMax(content.left(),
                          content.right() - bounded.width() + 1);
    const int maxY = qMax(content.top(),
                          content.bottom() - bounded.height() + 1);
    position.setX(qBound(content.left(), position.x(), maxX));
    position.setY(qBound(content.top(), position.y(), maxY));

    const QRect geometryRect(position, bounded);
    if (geometryRect == m_lastGeometry)
        return;
    m_lastGeometry = geometryRect;
    m_window->setGeometry(geometryRect);
}

void X11FollowAlongSurface::rememberPreviousFocus()
{
#if defined(UOS_AI_LIBRE_HAVE_X11_RECORD) \
    || defined(UOS_AI_LIBRE_HAVE_X11_SHORTCUT)
    m_hasPreviousFocus = false;
    if (Display *display = XOpenDisplay(nullptr)) {
        Window focus = 0;
        int revert = 0;
        XGetInputFocus(display, &focus, &revert);
        if (focus != 0 && focus != PointerRoot) {
            m_previousFocus = focus;
            m_hasPreviousFocus = true;
        }
        XCloseDisplay(display);
    }
#endif
}

void X11FollowAlongSurface::restorePreviousFocusIfNeeded()
{
    if (!m_window || !m_hasPreviousFocus)
        return;
#if defined(UOS_AI_LIBRE_HAVE_X11_RECORD) \
    || defined(UOS_AI_LIBRE_HAVE_X11_SHORTCUT)
    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return;
    Window focus = 0;
    int revert = 0;
    XGetInputFocus(display, &focus, &revert);
    if (focus == static_cast<Window>(m_window->winId())
        && m_previousFocus != focus) {
        XSetInputFocus(display, m_previousFocus, RevertToParent, CurrentTime);
        XFlush(display);
    }
    XCloseDisplay(display);
#endif
}

void X11FollowAlongSurface::windowExposed()
{
    if (!m_window || m_lastRequestId == 0)
        return;
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

void X11FollowAlongSurface::dismiss()
{
    if (!m_window)
        return;
    releaseSwapchain();
    m_window->hide();
    FollowAlongPresentationState state;
    state.accepted = true;
    state.overlayEffective = true;
    state.focused = false;
    state.geometry = geometry();
    postPresentation(PresentationEventType::Dismissed, m_lastRequestId, state);
}

void X11FollowAlongSurface::requestTextInputFocus()
{
    if (!m_window)
        return;
#if defined(UOS_AI_LIBRE_HAVE_X11_RECORD) \
    || defined(UOS_AI_LIBRE_HAVE_X11_SHORTCUT)
    if (Display *display = XOpenDisplay(nullptr)) {
        XSetInputFocus(display, static_cast<Window>(m_window->winId()),
                       RevertToParent, CurrentTime);
        XFlush(display);
        XCloseDisplay(display);
    }
#else
    m_window->requestActivate();
#endif
}

QRect X11FollowAlongSurface::geometry() const
{
    if (!m_window)
        return m_lastGeometry;
    return m_window->geometry();
}

bool X11FollowAlongSurface::isFocused() const
{
    return m_window && m_window->isActive();
}
