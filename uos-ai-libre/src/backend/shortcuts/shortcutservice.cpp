#include "shortcutservice.h"

#include "ddeshortcutbackend.h"
#include "shortcutbackend.h"
#include "x11shortcutbackend.h"

#include <QGuiApplication>

#include <memory>

namespace {

class UnavailableShortcutBackend final : public ShortcutBackend {
public:
    QString name() const override { return QStringLiteral("unavailable"); }
    bool available() const override { return false; }
    bool registerShortcut(const QString &, const QString &, const QString &,
                          QString *error) override {
        if (error)
            *error = QStringLiteral("当前桌面环境没有可用的全局快捷键后端");
        return false;
    }
    bool unregisterShortcut(const QString &, QString *) override {
        return false;
    }
};

bool isWaylandSession() {
    const QString platform = QGuiApplication::platformName().toLower();
    return platform.contains(QStringLiteral("wayland"))
           || platform.contains(QStringLiteral("treeland"));
}

} // namespace

ShortcutService::ShortcutService() = default;
ShortcutService::~ShortcutService() = default;

void ShortcutService::ensureBackend() {
    if (m_backend && m_backend->available())
        return;

    if (isWaylandSession()) {
        auto treeland = std::make_unique<TreelandShortcutBackend>();
        if (treeland->available()) {
            m_backend = std::move(treeland);
            return;
        }
    } else {
        // A DDE session provides the keybinding service on X11 too. Prefer it
        // over a second XGrabKey registration, which would conflict with the
        // desktop's existing grab.
        auto dde = std::make_unique<DdeShortcutBackend>();
        if (dde->available()) {
            m_backend = std::move(dde);
            return;
        }
        auto x11 = std::make_unique<X11ShortcutBackend>();
        if (x11->available()) {
            m_backend = std::move(x11);
            return;
        }
    }
    m_backend = std::make_unique<UnavailableShortcutBackend>();
}

bool ShortcutService::registerShortcut(const QString &id,
                                       const QString &sequence,
                                       const QString &command, QString *error) {
    ensureBackend();
    return m_backend->registerShortcut(id, sequence, command, error);
}

bool ShortcutService::unregisterShortcut(const QString &id, QString *error) {
    ensureBackend();
    return m_backend->unregisterShortcut(id, error);
}

QString ShortcutService::backendName() const {
    return m_backend ? m_backend->name() : QStringLiteral("uninitialized");
}
