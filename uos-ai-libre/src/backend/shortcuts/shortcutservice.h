#pragma once

#include <QString>

#include <memory>

class ShortcutBackend;

/// Platform-neutral global shortcut entry point.
class ShortcutService {
public:
    ShortcutService();
    ~ShortcutService();

    ShortcutService(const ShortcutService &) = delete;
    ShortcutService &operator=(const ShortcutService &) = delete;

    bool registerShortcut(const QString &id, const QString &sequence,
                          const QString &command, QString *error = nullptr);
    bool unregisterShortcut(const QString &id, QString *error = nullptr);
    QString backendName() const;

private:
    void ensureBackend();

    std::unique_ptr<ShortcutBackend> m_backend;
};
