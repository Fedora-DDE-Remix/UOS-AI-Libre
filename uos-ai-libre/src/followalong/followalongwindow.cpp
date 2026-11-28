// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongwindow.h"

#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
#include "treelandoverlay.h"
#endif

#include <QCursor>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>

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

FollowAlongWindowHost::FollowAlongWindowHost(QQmlEngine *engine,
                                             QObject *parent)
    : QObject(parent), m_engine(engine)
{
}

FollowAlongWindowHost::~FollowAlongWindowHost() { destroyWindow(); }

bool FollowAlongWindowHost::visible() const
{
    return m_window && m_window->isVisible();
}

QRect FollowAlongWindowHost::geometry() const
{
    if (!m_window)
        return {};
    return QRect(m_window->position(), m_window->size());
}

bool FollowAlongWindowHost::cursorInside() const
{
    if (!m_window)
        return false;
    return geometry().contains(QCursor::pos());
}

bool FollowAlongWindowHost::cursorPositionAvailable() const
{
    // QCursor::pos() is not meaningful for Wayland clients.
    return !QGuiApplication::platformName().toLower()
                .contains(QStringLiteral("wayland"));
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

    connect(window, SIGNAL(actionTriggered(int, bool)), this,
            SLOT(onActionTriggered(int, bool)));
    connect(window, SIGNAL(dismissed()), this, SLOT(onDismissed()));
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
    }

#ifdef UOS_AI_LIBRE_FOLLOWALONG_WAYLAND
    if (isWayland)
        m_overlay = TreelandOverlay::attach(window);
#endif
}

void FollowAlongWindowHost::show(const QString &text, const QPoint &globalPos,
                                 const QVariantList &actions)
{
    ensureWindow();
    if (!m_window)
        return;

    m_text = text;
    m_anchor = globalPos;
    m_window->setProperty("selectedText", text);
    m_window->setProperty("actions", actions);
    qInfo() << "FollowAlong: show toolbar text" << text.size()
            << "at" << globalPos << "overlay" << (m_overlay != nullptr);

    const QString platform = QGuiApplication::platformName().toLower();
    const bool isWayland = platform.contains(QStringLiteral("wayland"));
    if (!isWayland)
        positionX11();

    m_window->show();
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
    QPoint pos(m_anchor.x() - size.width() / 2, m_anchor.y() + 12);
    if (QScreen *screen = QGuiApplication::screenAt(m_anchor)) {
        const QRect area = screen->availableGeometry();
        if (pos.y() + size.height() > area.bottom())
            pos.setY(m_anchor.y() - size.height() - 12);
        pos.setX(qBound(area.left() + 6, pos.x(),
                        area.right() - size.width() - 6));
        pos.setY(qBound(area.top() + 6, pos.y(),
                        area.bottom() - size.height() - 6));
    }
    return pos;
}

void FollowAlongWindowHost::positionX11(const QSize &size)
{
    if (!m_window)
        return;
    m_window->setPosition(positionForSize(size));
}

void FollowAlongWindowHost::setSize(const QSize &size)
{
    if (!m_window)
        return;
    // Set position and size in one configure request. Moving after a resize
    // lets the window manager constrain the width at the old screen edge.
    m_window->setGeometry(QRect(positionForSize(size), size));
}

void FollowAlongWindowHost::setExpanded(bool expanded)
{
    if (m_window)
        m_window->setProperty("expanded", expanded);
}

void FollowAlongWindowHost::destroyWindow()
{
    if (!m_window)
        return;
    QQuickWindow *window = m_window;
    m_window = nullptr;
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
    if (m_window)
        m_window->hide();
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
