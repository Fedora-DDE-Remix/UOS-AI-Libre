#pragma once

#include <QString>

/// Platform/desktop backend for registering global shortcuts.
///
/// The caller supplies a stable logical id, a QKeySequence portable-text
/// sequence, and a command to launch. X11, Treeland and the DDE keybinding
/// service can implement this without leaking their native types into the UI.
class ShortcutBackend {
public:
    virtual ~ShortcutBackend() = default;

    virtual QString name() const = 0;
    virtual bool available() const = 0;
    virtual bool registerShortcut(const QString &id, const QString &sequence,
                                  const QString &command,
                                  QString *error) = 0;
    virtual bool unregisterShortcut(const QString &id, QString *error) = 0;
};
