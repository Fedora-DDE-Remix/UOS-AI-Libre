// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "treelandoverlay.h"

#include <QEvent>
#include <QLoggingCategory>
#include <QPlatformSurfaceEvent>
#include <QWindow>
#include <QWaylandClientExtension>

#include <private/qwaylandwindow_p.h>

#include <qwayland-treeland-dde-shell-v1.h>

Q_LOGGING_CATEGORY(logFollowAlongTreeland, "uosai.followalong.treeland")

namespace {

class TreelandShellManager final
    : public QWaylandClientExtensionTemplate<TreelandShellManager>
    , public QtWayland::treeland_dde_shell_manager_v1
{
public:
    TreelandShellManager()
        : QWaylandClientExtensionTemplate<TreelandShellManager>(2)
    {
        initialize();
    }
};

class TreelandShellSurface final
    : public QtWayland::treeland_dde_shell_surface_v1
{
public:
    explicit TreelandShellSurface(struct ::treeland_dde_shell_surface_v1 *id)
        : QtWayland::treeland_dde_shell_surface_v1(id)
    {
    }

    ~TreelandShellSurface() override { destroy(); }
};

TreelandShellManager *shellManager()
{
    static TreelandShellManager manager;
    return &manager;
}

} // namespace

TreelandOverlay::TreelandOverlay(QWindow *window)
    : QObject(window)
    , m_window(window)
{
    m_window->installEventFilter(this);

    TreelandShellManager *manager = shellManager();
    connect(manager, &QWaylandClientExtension::activeChanged, this,
            [this, manager]() {
                if (manager->isActive())
                    attachSurface();
            });

    if (m_window->handle())
        attachSurface();
}

TreelandOverlay::~TreelandOverlay()
{
    delete static_cast<TreelandShellSurface *>(m_surface);
    m_surface = nullptr;

}

TreelandOverlay *TreelandOverlay::attach(QWindow *window)
{
    if (!window)
        return nullptr;
    return new TreelandOverlay(window);
}

bool TreelandOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::PlatformSurface) {
        auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
        if (surfaceEvent->surfaceEventType()
            == QPlatformSurfaceEvent::SurfaceCreated) {
            attachSurface();
        }
    }
    return QObject::eventFilter(watched, event);
}

void TreelandOverlay::attachSurface()
{
    if (m_surface)
        return;

    auto *manager = shellManager();
    if (!manager->isActive()) {
        qCInfo(logFollowAlongTreeland)
            << "waiting for treeland_dde_shell_manager_v1";
        return;
    }

    auto *window = m_window->nativeInterface<
        QNativeInterface::Private::QWaylandWindow>();
    if (!window || !window->surface()) {
        qCWarning(logFollowAlongTreeland) << "no wl_surface for toolbar";
        return;
    }

    auto *shellSurface = manager->get_shell_surface(window->surface());
    if (!shellSurface) {
        qCWarning(logFollowAlongTreeland) << "get_shell_surface failed";
        return;
    }

    auto *surface = new TreelandShellSurface(shellSurface);
    surface->set_role(
        QtWayland::treeland_dde_shell_surface_v1::role_overlay);
    // Placement is a one-shot hint in Treeland. Apply the explicit position
    // when we know the real cursor position (through XWayland); otherwise
    // let the compositor center the surface below the cursor.
    if (m_position.has_value())
        surface->set_surface_position(m_position->x(), m_position->y());
    else if (m_autoPlacement.has_value())
        surface->set_auto_placement(*m_autoPlacement);
    surface->set_skip_switcher(1);
    surface->set_skip_dock_preview(1);
    surface->set_skip_muti_task_view(1);
    surface->set_accept_keyboard_focus(0);
    m_surface = surface;
    if (m_position.has_value())
        qCInfo(logFollowAlongTreeland) << "overlay attached at" << *m_position;
    else
        qCInfo(logFollowAlongTreeland) << "overlay attached with auto placement";
}

void TreelandOverlay::setKeyboardFocus(bool accepted)
{
    auto *surface = static_cast<TreelandShellSurface *>(m_surface);
    if (!surface)
        return;
    surface->set_accept_keyboard_focus(accepted ? 1 : 0);
}

void TreelandOverlay::setPosition(const QPoint &position)
{
    if (m_position == position)
        return;
    m_position = position;
    if (auto *surface = static_cast<TreelandShellSurface *>(m_surface))
        surface->set_surface_position(position.x(), position.y());
}

void TreelandOverlay::setAutoPlacement(int yOffset)
{
    if (m_autoPlacement == yOffset)
        return;
    m_autoPlacement = yOffset;
    if (auto *surface = static_cast<TreelandShellSurface *>(m_surface))
        surface->set_auto_placement(static_cast<uint32_t>(yOffset));
}
