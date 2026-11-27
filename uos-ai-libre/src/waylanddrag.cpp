#include "waylanddrag.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QtCore/qglobal.h>

#if defined(UOS_AI_LIBRE_HAVE_WAYLAND_DRAG) && QT_CONFIG(wayland)
#include <QtGui/qguiapplication_platform.h>

#include <wayland-client.h>

#include <algorithm>
#include <cstring>

#include "relative-pointer-unstable-v1-client-protocol.h"

namespace {
constexpr uint32_t RelativePointerVersion = 1;

const wl_registry_listener kRegistryListener = {
    &WaylandDragTracker::registryGlobal,
    &WaylandDragTracker::registryGlobalRemove,
};

const zwp_relative_pointer_v1_listener kRelativePointerListener = {
    &WaylandDragTracker::relativeMotion,
};
} // namespace
#endif

WaylandDragTracker::WaylandDragTracker(QObject *parent) : QObject(parent) {
    if (qApp)
        qApp->installEventFilter(this);
#ifdef UOS_AI_LIBRE_HAVE_WAYLAND_DRAG
#if QT_CONFIG(wayland)
    auto *wayland =
        qGuiApp
            ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()
            : nullptr;
    if (!wayland)
        return;
    m_display = wayland->display();
    if (!m_display)
        return;
    m_registry = wl_display_get_registry(m_display);
    if (!m_registry)
        return;
    wl_registry_add_listener(m_registry, &kRegistryListener, this);
#endif
#endif
}

WaylandDragTracker::~WaylandDragTracker() {
    if (qApp)
        qApp->removeEventFilter(this);
    end();
#ifdef UOS_AI_LIBRE_HAVE_WAYLAND_DRAG
#if QT_CONFIG(wayland)
    destroyRelativePointer();
    if (m_manager) {
        zwp_relative_pointer_manager_v1_destroy(m_manager);
        m_manager = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
#endif
#endif
}

bool WaylandDragTracker::begin() {
    end();
#ifdef UOS_AI_LIBRE_HAVE_WAYLAND_DRAG
#if QT_CONFIG(wayland)
    createRelativePointer();
    if (!m_relative)
        return false;
    m_active = true;
    return true;
#else
    return false;
#endif
#else
    return false;
#endif
}

void WaylandDragTracker::end() {
    m_active = false;
}

bool WaylandDragTracker::eventFilter(QObject *watched, QEvent *event) {
    // If the release still reaches this application (implicit grab or a
    // window other than the quick panel), terminate the drag immediately.
    // A release delivered to another client is handled by the hover/button
    // fallback in QuickAiPanel.qml.
    if (m_active && event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            end();
            emit released();
        }
    }
    return QObject::eventFilter(watched, event);
}

#ifdef UOS_AI_LIBRE_HAVE_WAYLAND_DRAG
#if QT_CONFIG(wayland)
void WaylandDragTracker::registryGlobal(void *data, wl_registry *registry,
                                        uint32_t name, const char *interface,
                                        uint32_t version) {
    auto *self = static_cast<WaylandDragTracker *>(data);
    if (!self || !interface)
        return;
    if (std::strcmp(interface, zwp_relative_pointer_manager_v1_interface.name)
        != 0)
        return;
    if (self->m_manager)
        return;
    self->m_manager = static_cast<zwp_relative_pointer_manager_v1 *>(
        wl_registry_bind(registry, name,
                         &zwp_relative_pointer_manager_v1_interface,
                         std::min(version, RelativePointerVersion)));
    // The seat/pointer normally exists by the time the registry event is
    // dispatched. Creating the object now avoids a first-drag setup gap; the
    // tracker ignores the deltas until begin() marks it active.
    self->createRelativePointer();
    self->updateAvailable();
}

void WaylandDragTracker::registryGlobalRemove(void *data, wl_registry *registry,
                                              uint32_t name) {
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(name)
}

void WaylandDragTracker::relativeMotion(
    void *data, zwp_relative_pointer_v1 *pointer, uint32_t timeHi,
    uint32_t timeLo, int32_t dx, int32_t dy, int32_t dxUnaccelerated,
    int32_t dyUnaccelerated) {
    Q_UNUSED(pointer)
    Q_UNUSED(timeHi)
    Q_UNUSED(timeLo)
    Q_UNUSED(dxUnaccelerated)
    Q_UNUSED(dyUnaccelerated)

    auto *self = static_cast<WaylandDragTracker *>(data);
    if (!self || !self->m_active)
        return;

    const qreal x = wl_fixed_to_double(dx);
    const qreal y = wl_fixed_to_double(dy);
    if (x != 0.0 || y != 0.0)
        emit self->moved(x, y);
}

void WaylandDragTracker::createRelativePointer() {
    if (!m_manager)
        return;

    auto *wayland =
        qGuiApp
            ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()
            : nullptr;
    if (!wayland)
        return;

    wl_pointer *pointer = wayland->pointer();
    if (!pointer)
        return;

    if (m_relative && m_pointer == pointer) {
        updateAvailable();
        return;
    }

    destroyRelativePointer();
    m_pointer = pointer;
    m_relative = zwp_relative_pointer_manager_v1_get_relative_pointer(
        m_manager, pointer);
    if (!m_relative) {
        updateAvailable();
        return;
    }
    zwp_relative_pointer_v1_add_listener(m_relative, &kRelativePointerListener,
                                         this);
    updateAvailable();
}

void WaylandDragTracker::destroyRelativePointer() {
    if (m_relative) {
        zwp_relative_pointer_v1_destroy(m_relative);
        m_relative = nullptr;
    }
    m_pointer = nullptr;
    updateAvailable();
}

void WaylandDragTracker::updateAvailable() {
    const bool available = m_relative != nullptr;
    if (m_available == available)
        return;
    m_available = available;
    emit availableChanged();
}
#endif
#endif
