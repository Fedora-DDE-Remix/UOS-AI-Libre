#pragma once

#include <QObject>

#include <cstdint>

class QEvent;

struct wl_display;
struct wl_pointer;
struct wl_registry;
struct zwp_relative_pointer_manager_v1;
struct zwp_relative_pointer_v1;

/// Delivers raw pointer deltas for dragging a layer-shell surface.
///
/// Generic wlr-layer-shell has no client-side move request: the compositor
/// positions the surface from anchors and margins. Emulating a drag with
/// surface-local mouse coordinates feeds the surface's own movement back into
/// the next delta, which makes the window oscillate under the cursor. The
/// relative-pointer protocol reports deltas that are independent of the
/// surface position, so it is the correct input source for this job.
///
/// The object is exposed to QML as `quickAiDrag`. `begin()` returns false
/// when the compositor does not advertise zwp_relative_pointer_manager_v1 (or
/// when the app is on X11); QML then keeps its ordinary local-coordinate drag.
class WaylandDragTracker final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    explicit WaylandDragTracker(QObject *parent = nullptr);
    ~WaylandDragTracker() override;

    bool available() const { return m_available; }

    /// Starts a drag. Returns true when relative-pointer deltas will drive it.
    Q_INVOKABLE bool begin();

    /// Stops accepting relative-pointer deltas.
    Q_INVOKABLE void end();

signals:
    void availableChanged();
    void moved(qreal dx, qreal dy);
    void released();

public:
    // Wayland C callbacks need to be accessible from the file-scope listener
    // tables; they are not part of the QML-facing API.
#ifdef UOS_AI_LIBRE_HAVE_WAYLAND_DRAG
    static void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t version);
    static void registryGlobalRemove(void *data, wl_registry *registry,
                                     uint32_t name);
    static void relativeMotion(void *data, zwp_relative_pointer_v1 *pointer,
                               uint32_t timeHi, uint32_t timeLo,
                               int32_t dx, int32_t dy,
                               int32_t dxUnaccelerated,
                               int32_t dyUnaccelerated);
#endif

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

#ifdef UOS_AI_LIBRE_HAVE_WAYLAND_DRAG
    void createRelativePointer();
    void destroyRelativePointer();
    void updateAvailable();
    void flushMotion();
#endif

#ifdef UOS_AI_LIBRE_HAVE_WAYLAND_DRAG
    qreal m_pendingDx = 0;
    qreal m_pendingDy = 0;
    bool m_flushScheduled = false;
#endif

    bool m_available = false;
    bool m_active = false;
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_pointer *m_pointer = nullptr;
    zwp_relative_pointer_manager_v1 *m_manager = nullptr;
    zwp_relative_pointer_v1 *m_relative = nullptr;
};
