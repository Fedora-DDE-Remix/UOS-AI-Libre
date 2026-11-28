// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongwindow.h"

#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
#include "treelandoverlay.h"
#endif

#include <QCursor>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QSGRendererInterface>
#include <QTimer>
#include <QWindow>

#include <limits>
#include <optional>

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

namespace {

constexpr int kEdgeMargin = 6;
constexpr int kToolbarGap = 12;

// Qt 6.8's Wayland plugin destroys the platform Vulkan surface before Qt
// Quick releases the QRhi swapchain. Hiding the FollowAlong QQuickWindow then
// makes Mesa abort with "queue ... destroyed while proxies still attached"
// and "Tried to add event to destroyed queue". Give Quick the destruction
// notification while the native surface is still alive, exactly as the main
// window does in windowhost.cpp.
bool releaseVulkanSwapchain(QQuickWindow *window)
{
    if (!window || !window->handle())
        return false;
    const QSGRendererInterface *renderer = window->rendererInterface();
    if (!renderer
        || renderer->graphicsApi() != QSGRendererInterface::Vulkan) {
        return false;
    }
    QPlatformSurfaceEvent event(
        QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed);
    QCoreApplication::sendEvent(window, &event);
    return true;
}

bool isWaylandPlatform()
{
    return QGuiApplication::platformName().toLower()
        .contains(QStringLiteral("wayland"));
}

#if defined(UOS_AI_LIBRE_HAVE_X11_SHORTCUT)
// Treeland does not expose a global pointer position to Wayland clients, but
// DDE always runs XWayland for X11 applications. The XWayland root tracks the
// real seat pointer, so it is the only reliable way to keep a native Wayland
// overlay inside the output without relying on compositor auto-placement.
// Only trust it when both coordinate spaces describe the same desktop.
std::optional<QPoint> xwaylandPointerPosition()
{
    if (qEnvironmentVariableIsEmpty("DISPLAY"))
        return std::nullopt;

    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return std::nullopt;

    Window root = DefaultRootWindow(display);
    Window child = 0;
    int rootX = 0;
    int rootY = 0;
    int winX = 0;
    int winY = 0;
    unsigned int mask = 0;
    const bool queried = XQueryPointer(display, root, &root, &child, &rootX,
                                       &rootY, &winX, &winY, &mask)
        != 0;
    const int rootWidth = DisplayWidth(display, DefaultScreen(display));
    const int rootHeight = DisplayHeight(display, DefaultScreen(display));
    XCloseDisplay(display);

    if (!queried)
        return std::nullopt;

    QRect virtualGeometry;
    for (QScreen *screen : QGuiApplication::screens())
        virtualGeometry = virtualGeometry.united(screen->virtualGeometry());
    if (virtualGeometry.isEmpty()
        || virtualGeometry.size() != QSize(rootWidth, rootHeight)) {
        // Mixed DPI or a non-standard XWayland layout would make the
        // coordinates ambiguous. Fall back to compositor auto-placement.
        return std::nullopt;
    }
    // XWayland's root starts at 0,0 for the bounding box of all outputs.
    // Translate back if Qt's virtual desktop uses negative coordinates.
    return QPoint(rootX + virtualGeometry.x(),
                  rootY + virtualGeometry.y());
}
#else
std::optional<QPoint> xwaylandPointerPosition()
{
    return std::nullopt;
}
#endif

} // namespace

FollowAlongWindowHost::FollowAlongWindowHost(QQmlEngine *engine,
                                             QObject *parent)
    : QObject(parent), m_engine(engine)
{
    // The main window uses the same workaround in WindowHost. FollowAlong can
    // be visible when the process exits, so release its Vulkan swapchain
    // before Qt starts destroying Wayland platform surfaces.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, [this]() { releaseSwapchain(); });
}

FollowAlongWindowHost::~FollowAlongWindowHost() { destroyWindow(); }

bool FollowAlongWindowHost::visible() const
{
    return m_window && m_window->isVisible();
}

qint64 FollowAlongWindowHost::lastLocalPressMs() const
{
    return m_lastLocalPressMs;
}

quint64 FollowAlongWindowHost::localPressSerial() const
{
    return m_localPressSerial;
}

quint64 FollowAlongWindowHost::generation() const
{
    return m_generation;
}

QRect FollowAlongWindowHost::geometry() const
{
    if (!m_window)
        return {};
    // Wayland clients do not see where the compositor mapped their surface.
    // Track the position we requested explicitly so outside-click checks can
    // still use exact geometry. X11 windows report their real position.
    if (isWaylandPlatform() && m_hasLastPosition)
        return QRect(m_lastPosition, m_window->size());
    return QRect(m_window->position(), m_window->size());
}

bool FollowAlongWindowHost::cursorPositionAvailable() const
{
    // This reports whether QCursor::pos() can be used for local positioning.
    // It stays false on Wayland even when the XWayland fallback can resolve
    // the global pointer, because the QWindow itself is still a Wayland
    // surface.
    return !isWaylandPlatform();
}

bool FollowAlongWindowHost::hasGlobalCursorPosition() const
{
    if (!isWaylandPlatform())
        return true;
    return xwaylandPointerPosition().has_value();
}

QPoint FollowAlongWindowHost::globalCursorPosition() const
{
    if (!isWaylandPlatform())
        return QCursor::pos();
    const std::optional<QPoint> position = xwaylandPointerPosition();
    return position.value_or(QPoint());
}

QScreen *FollowAlongWindowHost::screenForAnchor() const
{
    if (QScreen *screen = QGuiApplication::screenAt(m_anchor))
        return screen;
    if (m_window && m_window->screen())
        return m_window->screen();
    if (QScreen *primary = QGuiApplication::primaryScreen())
        return primary;

    QScreen *nearest = nullptr;
    qint64 nearestDistance = std::numeric_limits<qint64>::max();
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect rect = screen->geometry();
        const int dx = m_anchor.x() < rect.left()
            ? rect.left() - m_anchor.x()
            : (m_anchor.x() > rect.right() ? m_anchor.x() - rect.right() : 0);
        const int dy = m_anchor.y() < rect.top()
            ? rect.top() - m_anchor.y()
            : (m_anchor.y() > rect.bottom() ? m_anchor.y() - rect.bottom() : 0);
        const qint64 distance = qint64(dx) * dx + qint64(dy) * dy;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = screen;
        }
    }
    return nearest;
}

QRect FollowAlongWindowHost::availableRect() const
{
    if (QScreen *screen = screenForAnchor())
        return screen->availableGeometry();
    return QRect(0, 0, 1280, 720);
}

QSize FollowAlongWindowHost::clampedSize(const QSize &size) const
{
    const QRect area = availableRect();
    const QSize maximum(qMax(24, area.width() - 2 * kEdgeMargin),
                        qMax(24, area.height() - 2 * kEdgeMargin));
    return size.expandedTo(QSize(1, 1)).boundedTo(maximum);
}

int FollowAlongWindowHost::maxToolbarWidth() const
{
    return qMax(24, availableRect().width() - 2 * kEdgeMargin);
}

bool FollowAlongWindowHost::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::MouseButtonPress) {
        m_lastLocalPressMs = QDateTime::currentMSecsSinceEpoch();
        ++m_localPressSerial;
    }
    return QObject::eventFilter(watched, event);
}

void FollowAlongWindowHost::openMoreMenu()
{
    if (m_window)
        QMetaObject::invokeMethod(m_window, "openMoreMenu");
}

void FollowAlongWindowHost::activateForInput()
{
    if (!m_window)
        return;

    const QString platform = QGuiApplication::platformName().toLower();
    if (platform.contains(QStringLiteral("wayland"))) {
#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
        if (auto *overlay = qobject_cast<TreelandOverlay *>(m_overlay))
            overlay->setKeyboardFocus(true);
#endif
        m_window->requestActivate();
        return;
    }

#if defined(UOS_AI_LIBRE_HAVE_X11_RECORD) \
    || defined(UOS_AI_LIBRE_HAVE_X11_SHORTCUT)
    // The toolbar is an override-redirect X11 window (no window manager).
    // Clicking its text field must explicitly move the X input focus,
    // otherwise keystrokes continue going to the application behind it.
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

QQmlComponent *FollowAlongWindowHost::component()
{
    if (m_component)
        return m_component;

    m_component = new QQmlComponent(m_engine, this);
    m_component->loadFromModule(QStringLiteral("UosAiLibre"),
                                QStringLiteral("FollowAlongWindow"));
    if (m_component->isError()) {
        qWarning().noquote() << "failed to load FollowAlongWindow:"
                             << m_component->errorString();
        delete m_component;
        m_component = nullptr;
    }
    return m_component;
}

void FollowAlongWindowHost::ensureWindow()
{
    if (m_window)
        return;

    QQmlComponent *qml = component();
    if (!qml)
        return;

    QObject *object = qml->create(m_engine->rootContext());
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        if (object)
            object->deleteLater();
        return;
    }

    m_window = window;
    ++m_generation;
    const QString platform = QGuiApplication::platformName().toLower();
    const bool isWayland = platform.contains(QStringLiteral("wayland"));
    if (isWayland) {
        // Keyboard focus is controlled per input through Treeland's
        // set_accept_keyboard_focus request, so do not forbid it here.
        window->setFlags(Qt::Tool | Qt::FramelessWindowHint
                         | Qt::WindowStaysOnTopHint);
    } else {
        // Let the X11 window manager manage this window. An override-redirect
        // window cannot take keyboard focus reliably and Qt's popup menus do
        // not map correctly from one, so keep it a normal frameless tool
        // window and raise it explicitly after each show.
        window->setFlags(Qt::Tool | Qt::FramelessWindowHint
                         | Qt::WindowStaysOnTopHint);
    }

    window->installEventFilter(this);
    connect(window, SIGNAL(actionTriggered(int, bool)), this,
            SLOT(onActionTriggered(int, bool)));
    connect(window, SIGNAL(dismissed()), this, SLOT(onDismissed()));
    connect(window, SIGNAL(expandedChanged()), this,
            SLOT(onExpandedChanged()));
    connect(window, &QQuickWindow::closing, this,
            [this](QQuickCloseEvent *) { releaseSwapchain(); });
    connect(window, &QObject::destroyed, this, [this, window]() {
        if (m_window == window) {
            m_window = nullptr;
            m_overlay = nullptr;
        }
    });
    if (!isWayland) {
        connect(window, &QQuickWindow::widthChanged, this,
                [this]() { positionX11(); });
        connect(window, &QQuickWindow::heightChanged, this,
                [this]() { positionX11(); });
    } else {
        connect(window, &QQuickWindow::widthChanged, this,
                [this]() { updateWaylandPosition(); });
        connect(window, &QQuickWindow::heightChanged, this,
                [this]() { updateWaylandPosition(); });
    }

#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
    if (isWayland)
        m_overlay = TreelandOverlay::attach(window);
#endif
}

void FollowAlongWindowHost::show(const QString &text, const QPoint &globalPos,
                                 const QVariantList &actions, bool expanded)
{
    const bool forcedAnchor = m_forceExplicitAnchor;
    m_forceExplicitAnchor = false;
    ensureWindow();
    if (!m_window)
        return;

    // A previous hide released the Vulkan swapchain while the Wayland
    // surface was still valid. Qt Quick will create a fresh swapchain when
    // the window is shown and rendered again.
    m_swapchainReleased = false;
    const bool isWayland = isWaylandPlatform();
    std::optional<QPoint> waylandPosition;
    if (isWayland) {
        // A caller-supplied position is a valid explicit anchor. It is used
        // by the preview/test hook, by an expanded rebuild that already
        // resolved the anchor, and by selection monitors that resolved a
        // global position. QPoint(0,0) is a legitimate corner, so do not use
        // QPoint::isNull() as the only "known" test.
        const bool callerAnchor = forcedAnchor || !globalPos.isNull()
            || qEnvironmentVariableIsSet(
                "UOS_AI_LIBRE_FOLLOWALONG_PREVIEW_ANCHOR");
        if (callerAnchor)
            waylandPosition = globalPos;
        else
            waylandPosition = xwaylandPointerPosition();
    }

    m_text = text;
    m_anchor = waylandPosition.value_or(globalPos);
    m_exactAnchor = !isWayland || waylandPosition.has_value();
    m_showing = true;
    m_window->setProperty("selectedText", text);
    m_window->setProperty("actions", actions);
    if (expanded)
        m_window->setProperty("expanded", true);
    qInfo() << "FollowAlong: show toolbar text" << text.size()
            << "at" << m_anchor << "overlay" << (m_overlay != nullptr)
            << "exact" << m_exactAnchor << "expanded" << expanded;

    if (!isWayland) {
        positionX11();
    } else {
        const QSize size = clampedSize(m_window->size());
        if (size != m_window->size())
            m_window->resize(size);
        const QPoint position = positionForSize(size);
        const bool exact = waylandPosition.has_value();
        m_lastPosition = position;
        m_hasLastPosition = exact;
#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
        if (auto *overlay = qobject_cast<TreelandOverlay *>(m_overlay)) {
            if (exact)
                overlay->setPosition(position);
            else
                overlay->setAutoPlacement(1);
        }
#endif
    }

    m_window->show();
    m_showing = false;
    // With X11BypassWindowManagerHint the window is not managed, so showing
    // it again can leave it below the application window. Raise explicitly;
    // otherwise the toolbar is visible but cannot receive clicks.
    if (!isWayland)
        m_window->raise();
}

void FollowAlongWindowHost::positionX11()
{
    if (!m_window)
        return;
    positionX11(m_window->size());
}

QPoint FollowAlongWindowHost::positionForSize(const QSize &size) const
{
    // The user asked for the toolbar below the cursor. If there is not enough
    // room at the bottom of the output, flip it above the cursor and clamp all
    // edges; never let the pill run off the screen.
    const QSize bounded = clampedSize(size);
    const QRect area = availableRect();
    const QRect content = area.adjusted(kEdgeMargin, kEdgeMargin,
                                        -kEdgeMargin, -kEdgeMargin);
    const int maxX = qMax(content.left(),
                          content.right() - bounded.width() + 1);
    const int maxY = qMax(content.top(),
                          content.bottom() - bounded.height() + 1);

    QPoint pos(m_anchor.x() - bounded.width() / 2,
               m_anchor.y() + kToolbarGap);
    if (pos.y() > maxY)
        pos.setY(m_anchor.y() - bounded.height() - kToolbarGap);
    pos.setX(qBound(content.left(), pos.x(), maxX));
    pos.setY(qBound(content.top(), pos.y(), maxY));
    return pos;
}

void FollowAlongWindowHost::positionX11(const QSize &size)
{
    if (!m_window)
        return;
    const QSize bounded = clampedSize(size);
    const QPoint position = positionForSize(bounded);
    m_lastPosition = position;
    m_hasLastPosition = true;
    m_window->setPosition(position);
}

void FollowAlongWindowHost::setSize(const QSize &size)
{
    if (!m_window)
        return;
    // Set position and size in one configure request. Moving after a resize
    // lets the window manager constrain the width at the old screen edge.
    const QSize bounded = clampedSize(size);
    const QPoint position = positionForSize(bounded);
    m_lastPosition = position;
    m_hasLastPosition = true;
    m_window->setGeometry(QRect(position, bounded));
}

void FollowAlongWindowHost::updateWaylandPosition()
{
    if (!m_window || !isWaylandPlatform())
        return;

    const QSize bounded = clampedSize(m_window->size());
    if (bounded != m_window->size())
        m_window->resize(bounded);
    if (!m_exactAnchor)
        return;

    const QPoint position = positionForSize(bounded);
    m_lastPosition = position;
    m_hasLastPosition = true;
#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
    if (auto *overlay = qobject_cast<TreelandOverlay *>(m_overlay))
        overlay->setPosition(position);
#endif
}

void FollowAlongWindowHost::setExpanded(bool expanded)
{
    if (m_window)
        m_window->setProperty("expanded", expanded);
}

void FollowAlongWindowHost::releaseSwapchain()
{
    if (m_swapchainReleased || !m_window)
        return;
    if (releaseVulkanSwapchain(m_window))
        m_swapchainReleased = true;
}

void FollowAlongWindowHost::destroyWindow()
{
    if (!m_window)
        return;
    if (!m_swapchainReleased)
        releaseSwapchain();
    QQuickWindow *window = m_window;
    m_window = nullptr;
    ++m_generation;
    // treeland_dde_shell_surface_v1.destroy() must be sent before the
    // wl_surface disappears. The overlay is a QObject child of the window, so
    // delete it explicitly before closing the platform window.
    delete m_overlay;
    m_overlay = nullptr;
    window->close();
    window->deleteLater();
}

void FollowAlongWindowHost::hide()
{
#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
    if (auto *overlay = qobject_cast<TreelandOverlay *>(m_overlay))
        overlay->setKeyboardFocus(false);
#endif
    if (m_window) {
        if (m_window->isVisible())
            releaseSwapchain();
        m_window->hide();
    }
}

void FollowAlongWindowHost::onActionTriggered(int type, bool custom)
{
    const QString text = m_text;
    hide();
    Q_EMIT actionTriggered(type, custom, text);
}

void FollowAlongWindowHost::onDismissed()
{
    hide();
    Q_EMIT dismissed();
}

void FollowAlongWindowHost::onExpandedChanged()
{
    if (!m_window || !isWaylandPlatform() || m_showing || m_rebuildPending)
        return;
    if (!m_window->property("expanded").toBool())
        return;

    // Treeland applies dde-shell placement hints only when the surface is
    // first mapped. Expanding the same surface would keep the 24 px icon's
    // top-left corner and push the 425 px pill off the right/bottom edge.
    // Recreate the overlay at the expanded size so the clamped position is
    // applied before the new surface is mapped.
    m_rebuildPending = true;
    qInfo() << "FollowAlong: rebuilding Wayland overlay for expanded toolbar"
            << "at" << m_anchor;
    const QString text = m_text;
    const QPoint anchor = m_anchor;
    const QVariantList actions = m_window->property("actions").toList();
    // Hide the still-icon-sized surface immediately; otherwise it would be
    // briefly visible with the expanded content clipped at the old top-left.
    hide();
    QTimer::singleShot(0, this, [this, text, anchor, actions]() {
        m_rebuildPending = false;
        if (!m_window)
            return;
        destroyWindow();
        // The anchor was resolved by the existing window; (0,0) is a valid
        // corner and must not be re-interpreted as "unknown".
        m_forceExplicitAnchor = true;
        show(text, anchor, actions, true);
    });
}
