// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongview.h"

#include "followalongeventbus.h"
#include "x11followalongsurface.h"

#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
#include "treelandddefollowalongsurface.h"
#endif

#include <QCoreApplication>
#include <QCursor>
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

namespace {

constexpr int kEdgeMargin = 6;
constexpr int kToolbarGap = 12;
constexpr qint64 kOutsidePressCorrelationMs = 90;
constexpr qint64 kOwnPressWindowMs = 200;

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

} // namespace

FollowAlongSurface::FollowAlongSurface(QQmlEngine *engine,
                                       FollowAlongEventBus *bus,
                                       QObject *parent)
    : QObject(parent), m_engine(engine), m_bus(bus)
{
    connectBus();
    connect(this, &FollowAlongSurface::moreMenuRequested, this,
            &FollowAlongSurface::openMoreMenu);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, [this]() { releaseSwapchain(); });
}

FollowAlongSurface::~FollowAlongSurface() { destroyWindow(); }

FollowAlongSurface *FollowAlongSurface::create(QQmlEngine *engine,
                                               FollowAlongEventBus *bus,
                                               QObject *parent)
{
    const QString platform = QGuiApplication::platformName().toLower();
    if (platform.contains(QStringLiteral("wayland"))
        || platform.contains(QStringLiteral("treeland"))) {
#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
        return new TreelandDdeFollowAlongSurface(engine, bus, parent);
#else
        // Never fall back to an xdg-toplevel on Wayland: that is exactly the
        // behaviour this architecture must prevent.
        return nullptr;
#endif
    }
    return new X11FollowAlongSurface(engine, bus, parent);
}

void FollowAlongSurface::connectBus()
{
    if (!m_bus)
        return;
    connect(m_bus, &FollowAlongEventBus::selectionEvent, this,
            &FollowAlongSurface::onSelectionEvent);
}

bool FollowAlongSurface::visible() const
{
    return m_window && m_window->isVisible();
}

bool FollowAlongSurface::menuOpen() const
{
    return m_window && m_window->property("menuOpen").toBool();
}

int FollowAlongSurface::maxContentWidth() const
{
    const QRect area = availableRect(m_hasAnchor ? m_anchor : QPoint());
    return qMax(24, area.width() - 2 * kEdgeMargin);
}

QString FollowAlongSurface::backendName() const { return m_backendName; }

void FollowAlongSurface::openMoreMenu()
{
    qInfo() << "FollowAlong: open more menu requested";
    if (!m_window || !m_window->isVisible())
        return;
    if (!usesInlineMenu()) {
        QMetaObject::invokeMethod(m_window, "openMoreMenu");
        return;
    }
    const int contentHeight = m_window->property("menuHeight").toInt();
    const int menuHeight = contentHeight > 0 ? contentHeight : 205;
    const QPoint pill = positionForSize(
        m_anchor, QSize(m_window->width(), 40));
    const QRect area = availableRect(m_anchor);
    const bool above =
        pill.y() + 40 + 6 + menuHeight > area.bottom() - 6;
    m_window->setProperty("menuAbove", above);
    QMetaObject::invokeMethod(m_window, "openMoreMenu");
}

void FollowAlongSurface::closeMoreMenu()
{
    if (m_window)
        QMetaObject::invokeMethod(m_window, "closeMoreMenu");
}

QScreen *FollowAlongSurface::screenForAnchor(const QPoint &anchor) const
{
    if (QScreen *screen = QGuiApplication::screenAt(anchor))
        return screen;
    if (m_window && m_window->screen())
        return m_window->screen();
    if (QScreen *primary = QGuiApplication::primaryScreen())
        return primary;

    QScreen *nearest = nullptr;
    qint64 nearestDistance = std::numeric_limits<qint64>::max();
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect rect = screen->geometry();
        const int dx = anchor.x() < rect.left()
            ? rect.left() - anchor.x()
            : (anchor.x() > rect.right() ? anchor.x() - rect.right() : 0);
        const int dy = anchor.y() < rect.top()
            ? rect.top() - anchor.y()
            : (anchor.y() > rect.bottom() ? anchor.y() - rect.bottom() : 0);
        const qint64 distance = qint64(dx) * dx + qint64(dy) * dy;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = screen;
        }
    }
    return nearest;
}

QRect FollowAlongSurface::availableRect(const QPoint &anchor) const
{
    if (QScreen *screen = screenForAnchor(anchor))
        return screen->availableGeometry();
    return QRect(0, 0, 1280, 720);
}

QSize FollowAlongSurface::clampedSize(const QPoint &anchor,
                                      const QSize &size) const
{
    const QRect area = availableRect(anchor);
    const QSize maximum(qMax(24, area.width() - 2 * kEdgeMargin),
                        qMax(24, area.height() - 2 * kEdgeMargin));
    return size.expandedTo(QSize(1, 1)).boundedTo(maximum);
}

QPoint FollowAlongSurface::positionForSize(const QPoint &anchor,
                                           const QSize &size) const
{
    // Place the toolbar below the cursor. If there is not enough room at the
    // bottom of the output, flip it above the cursor and clamp all edges.
    const QSize bounded = clampedSize(anchor, size);
    const QRect area = availableRect(anchor);
    const QRect content = area.adjusted(kEdgeMargin, kEdgeMargin,
                                        -kEdgeMargin, -kEdgeMargin);
    const int maxX = qMax(content.left(),
                          content.right() - bounded.width() + 1);
    const int maxY = qMax(content.top(),
                          content.bottom() - bounded.height() + 1);

    QPoint pos(anchor.x() - bounded.width() / 2,
               anchor.y() + kToolbarGap);
    if (pos.y() > maxY)
        pos.setY(anchor.y() - bounded.height() - kToolbarGap);
    pos.setX(qBound(content.left(), pos.x(), maxX));
    pos.setY(qBound(content.top(), pos.y(), maxY));
    return pos;
}

QQuickWindow *FollowAlongSurface::createWindow(const QString &componentName)
{
    if (m_window)
        return m_window;

    if (!m_component) {
        m_component = new QQmlComponent(m_engine, this);
        m_component->loadFromModule(QStringLiteral("UosAiLibre"),
                                    componentName);
        if (m_component->isError()) {
            qWarning().noquote() << "failed to load" << componentName << ":"
                                 << m_component->errorString();
            delete m_component;
            m_component = nullptr;
        }
    }
    if (!m_component)
        return nullptr;

    QObject *object = m_component->create(m_engine->rootContext());
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        if (object)
            object->deleteLater();
        return nullptr;
    }

    m_window = window;
    m_exposeSeen = false;
    m_swapchainReleased = false;
    wireWindow(window);
    return window;
}

void FollowAlongSurface::wireWindow(QQuickWindow *window)
{
    window->installEventFilter(this);

    connect(window, SIGNAL(actionTriggered(int, bool)), this,
            SLOT(onWindowActionTriggered(int, bool)));
    connect(window, SIGNAL(askRequested(QString)), this,
            SLOT(onWindowAskRequested(QString)));
    connect(window, SIGNAL(expandRequested()), this,
            SIGNAL(expandRequested()));
    connect(window, SIGNAL(dismissRequested()), this,
            SIGNAL(dismissRequested()));
    connect(window, SIGNAL(settingsRequested()), this,
            SIGNAL(settingsRequested()));
    connect(window, SIGNAL(moreMenuRequested()), this,
            SIGNAL(moreMenuRequested()));

    connect(window, &QQuickWindow::closing, this,
            [this](QQuickCloseEvent *) { releaseSwapchain(); });
    connect(window, &QObject::destroyed, this, [this, window]() {
        if (m_window == window)
            m_window = nullptr;
    });
    connect(window, &QWindow::activeChanged, this,
            [this]() { emitFocusChanged(); });
    connect(window, &QQuickWindow::visibleChanged, this, [this, window]() {
        if (!window->isVisible()) {
            m_exposeSeen = false;
            m_pointerInside = false;
        }
    });
}

void FollowAlongSurface::applyContent(const FollowAlongContent &content)
{
    if (!m_window)
        return;
    m_content = content;
    m_window->setProperty("selectedText", content.selectedText);
    m_window->setProperty("actions", content.actions);
    m_window->setProperty("moreActions", content.moreActions);
    m_window->setProperty("expanded", content.expanded);
    m_window->setProperty("maxContentWidth", maxContentWidth());
}

void FollowAlongSurface::releaseSwapchain()
{
    if (m_swapchainReleased || !m_window)
        return;
    if (releaseVulkanSwapchain(m_window))
        m_swapchainReleased = true;
}

void FollowAlongSurface::destroyWindow()
{
    if (!m_window)
        return;
    if (!m_swapchainReleased)
        releaseSwapchain();
    QQuickWindow *window = m_window;
    m_window = nullptr;
    window->close();
    window->deleteLater();
}

void FollowAlongSurface::postPresentation(
    PresentationEventType type, quint64 requestId,
    const FollowAlongPresentationState &state, const QString &error)
{
    if (!m_bus)
        return;
    PresentationEvent event;
    event.type = type;
    event.requestId = requestId;
    event.state = state;
    event.state.backendName = m_backendName;
    event.error = error;
    m_bus->postPresentationEvent(event);
}

void FollowAlongSurface::noteOwnSurfaceInteraction()
{
    m_lastOwnInteractionMs = QDateTime::currentMSecsSinceEpoch();
    if (m_bus)
        m_bus->noteOwnSurfaceInteraction(m_lastOwnInteractionMs);
}

bool FollowAlongSurface::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        if (event->type() == QEvent::MouseButtonPress) {
            m_lastLocalPressMs = QDateTime::currentMSecsSinceEpoch();
            ++m_localPressSerial;
            noteOwnSurfaceInteraction();
        } else if (event->type() == QEvent::PlatformSurface) {
            auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
            if (surfaceEvent->surfaceEventType()
                == QPlatformSurfaceEvent::SurfaceCreated) {
                windowSurfaceCreated();
            }
        } else if (event->type() == QEvent::Expose) {
            const bool firstExpose = !m_exposeSeen;
            m_exposeSeen = true;
            if (firstExpose)
                windowExposed();
        } else if (event->type() == QEvent::Enter) {
            m_pointerInside = true;
        } else if (event->type() == QEvent::Leave) {
            m_pointerInside = false;
        } else if (event->type() == QEvent::FocusIn) {
            noteOwnSurfaceInteraction();
        }
    }
    return QObject::eventFilter(watched, event);
}

void FollowAlongSurface::onWindowActionTriggered(int type, bool custom)
{
    Q_EMIT actionTriggered(type, custom, m_content.selectedText);
}

void FollowAlongSurface::onWindowAskRequested(const QString &question)
{
    Q_EMIT askRequested(question, m_content.selectedText);
}

void FollowAlongSurface::emitFocusChanged()
{
    if (!m_window || m_lastRequestId == 0)
        return;
    FollowAlongPresentationState state;
    state.focused = isFocused();
    state.geometry = geometry();
    qInfo() << "FollowAlong: focus changed" << state.focused
            << "backend" << m_backendName;
    postPresentation(PresentationEventType::FocusChanged, m_lastRequestId,
                     state);
}

void FollowAlongSurface::onSelectionEvent(const SelectionEvent &event)
{
    if (event.phase != SelectionPhase::GestureBegan)
        return;
    if (!visible())
        return;
    if (menuOpen() && !usesInlineMenu())
        return;

    // Pointer Enter/Leave is the most reliable "does the pointer belong to
    // this surface" signal. It does not depend on XWayland coordinates, which
    // can be stale on a nested compositor, and it is already ordered with the
    // compositor's pointer focus.
    if (m_pointerInside)
        return;
    scheduleOutsideCheck(event);
}

void FollowAlongSurface::scheduleOutsideCheck(const SelectionEvent &event)
{
    // Without a global pointer position, correlate the active-notify press
    // with a local press on our own surface. The local event can arrive
    // slightly before or after the global notification.
    const quint64 sequence = ++m_outsidePressSequence;
    m_pendingOutsideSequence = sequence;
    const qint64 pressedAt = event.timestampMs > 0
        ? event.timestampMs
        : QDateTime::currentMSecsSinceEpoch();
    const quint64 localSerial = m_localPressSerial;

    QTimer::singleShot(kOutsidePressCorrelationMs, this,
                       [this, sequence, pressedAt, localSerial]() {
        if (m_pendingOutsideSequence != sequence)
            return;
        if (!m_window || !m_window->isVisible())
            return;
        if (m_pointerInside)
            return;
        // A local press may arrive before or after the global notification.
        if (m_localPressSerial != localSerial
            || m_lastLocalPressMs >= pressedAt - kOwnPressWindowMs) {
            return;
        }
        qInfo() << "FollowAlong: outside click (correlated), hiding";
        Q_EMIT dismissRequested();
    });
}
