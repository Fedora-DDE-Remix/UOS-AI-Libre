#include "x11shortcutbackend.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QStringList>

#ifdef UOS_AI_LIBRE_HAVE_X11_SHORTCUT
#include <QtGui/qguiapplication_platform.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>

namespace {

Display *x11Display() {
    auto *app = qGuiApp
                    ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>()
                    : nullptr;
    return app ? app->display() : nullptr;
}

unsigned int modifierMask(const QString &part) {
    const QString value = part.trimmed().toLower();
    if (value == QStringLiteral("ctrl") || value == QStringLiteral("control"))
        return ControlMask;
    if (value == QStringLiteral("alt"))
        return Mod1Mask;
    if (value == QStringLiteral("shift"))
        return ShiftMask;
    if (value == QStringLiteral("meta") || value == QStringLiteral("super")
        || value == QStringLiteral("win"))
        return Mod4Mask;
    return 0;
}

bool parseSequence(const QString &sequence, unsigned int *keycode,
                   unsigned int *modifiers, QString *error) {
    Display *display = x11Display();
    if (!display) {
        if (error)
            *error = QStringLiteral("无法连接 X11 显示");
        return false;
    }
    const QStringList parts = sequence.split(QLatin1Char('+'));
    if (parts.isEmpty()) {
        if (error)
            *error = QStringLiteral("快捷键为空");
        return false;
    }
    unsigned int mods = 0;
    for (int i = 0; i + 1 < parts.size(); ++i) {
        const unsigned int mask = modifierMask(parts.at(i));
        if (mask == 0) {
            if (error)
                *error = QStringLiteral("未知修饰键：%1").arg(parts.at(i));
            return false;
        }
        mods |= mask;
    }
    QString key = parts.constLast().trimmed();
    KeySym symbol = XStringToKeysym(key.toUtf8().constData());
    if (symbol == NoSymbol)
        symbol = XStringToKeysym(key.toLower().toUtf8().constData());
    if (symbol == NoSymbol) {
        if (error)
            *error = QStringLiteral("未知按键：%1").arg(key);
        return false;
    }
    const KeyCode code = XKeysymToKeycode(display, symbol);
    if (code == 0) {
        if (error)
            *error = QStringLiteral("按键没有对应的 keycode：%1").arg(key);
        return false;
    }
    *keycode = code;
    *modifiers = mods;
    return true;
}

} // namespace

X11ShortcutBackend::~X11ShortcutBackend() {
    if (m_filterInstalled)
        qApp->removeNativeEventFilter(this);
}

QString X11ShortcutBackend::name() const {
    return QStringLiteral("x11");
}

bool X11ShortcutBackend::available() const {
    return x11Display() != nullptr;
}

bool X11ShortcutBackend::registerShortcut(const QString &id,
                                          const QString &sequence,
                                          const QString &command,
                                          QString *error) {
    if (error)
        error->clear();
    Display *display = x11Display();
    if (!display) {
        if (error)
            *error = QStringLiteral("无法连接 X11 显示");
        return false;
    }
    if (id.isEmpty() || command.isEmpty()) {
        if (error)
            *error = QStringLiteral("快捷键参数为空");
        return false;
    }
    unregisterShortcut(id, nullptr);

    unsigned int keycode = 0;
    unsigned int modifiers = 0;
    if (!parseSequence(sequence, &keycode, &modifiers, error))
        return false;

    static bool grab_failed = false;
    grab_failed = false;
    auto handler = [](Display *, XErrorEvent *event) -> int {
        if (event && event->error_code == BadAccess)
            grab_failed = true;
        return 0;
    };
    XErrorHandler old_handler = XSetErrorHandler(handler);
    const unsigned int lock_variants[] = {
        0, LockMask, Mod2Mask, LockMask | Mod2Mask,
    };
    for (unsigned int variant : lock_variants) {
        XGrabKey(display, keycode, modifiers | variant,
                 DefaultRootWindow(display), True, GrabModeAsync,
                 GrabModeAsync);
    }
    XSync(display, False);
    XSetErrorHandler(old_handler);
    if (grab_failed) {
        for (unsigned int variant : lock_variants) {
            XUngrabKey(display, keycode, modifiers | variant,
                       DefaultRootWindow(display));
        }
        XSync(display, False);
        if (error)
            *error = QStringLiteral("快捷键已被其他程序占用");
        return false;
    }

    if (!m_filterInstalled) {
        qApp->installNativeEventFilter(this);
        m_filterInstalled = true;
    }
    m_bindings.append({id, keycode, modifiers, command});
    return true;
}

bool X11ShortcutBackend::unregisterShortcut(const QString &id, QString *error) {
    if (error)
        error->clear();
    Display *display = x11Display();
    if (!display)
        return false;
    for (int i = m_bindings.size() - 1; i >= 0; --i) {
        const Binding &binding = m_bindings.at(i);
        if (binding.id != id)
            continue;
        const unsigned int lock_variants[] = {
            0u, LockMask, Mod2Mask, LockMask | Mod2Mask,
        };
        for (unsigned int variant : lock_variants) {
            XUngrabKey(display, binding.keycode, binding.modifiers | variant,
                       DefaultRootWindow(display));
        }
        m_bindings.removeAt(i);
        XSync(display, False);
        return true;
    }
    return false;
}

bool X11ShortcutBackend::nativeEventFilter(const QByteArray &eventType,
                                           void *message, qintptr *result) {
    Q_UNUSED(result)
    if (eventType != "xcb_generic_event_t" || !message)
        return false;
    auto *event = static_cast<xcb_generic_event_t *>(message);
    if ((event->response_type & ~0x80) != XCB_KEY_PRESS)
        return false;
    auto *key = reinterpret_cast<xcb_key_press_event_t *>(event);
    const unsigned int state = key->state & ~(LockMask | Mod2Mask);
    for (const Binding &binding : m_bindings) {
        if (binding.keycode != key->detail || binding.modifiers != state)
            continue;
        const QStringList parts = QProcess::splitCommand(binding.command);
        if (!parts.isEmpty())
            QProcess::startDetached(parts.first(), parts.mid(1));
        return false;
    }
    return false;
}

#else

X11ShortcutBackend::~X11ShortcutBackend() {
    if (m_filterInstalled)
        qApp->removeNativeEventFilter(this);
}

QString X11ShortcutBackend::name() const {
    return QStringLiteral("x11");
}

bool X11ShortcutBackend::available() const {
    return false;
}

bool X11ShortcutBackend::registerShortcut(const QString &, const QString &,
                                          const QString &, QString *error) {
    if (error)
        *error = QStringLiteral("当前构建未启用 X11 快捷键后端");
    return false;
}

bool X11ShortcutBackend::unregisterShortcut(const QString &, QString *) {
    return false;
}

bool X11ShortcutBackend::nativeEventFilter(const QByteArray &, void *,
                                           qintptr *) {
    return false;
}

#endif
