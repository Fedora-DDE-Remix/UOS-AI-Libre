#pragma once

#include "shortcutbackend.h"

#include <QHash>
#include <QString>

/// DDE keybinding-service backend (org.deepin.dde.Keybinding1).
///
/// This is the supported shortcut integration on a DDE session. It is also
/// the manager for Treeland's compositor shortcuts, so the Treeland backend
/// below is a thin platform policy over the same service.
class DdeShortcutBackend : public ShortcutBackend {
public:
    QString name() const override;
    bool available() const override;
    bool registerShortcut(const QString &id, const QString &sequence,
                          const QString &command, QString *error) override;
    bool unregisterShortcut(const QString &id, QString *error) override;

private:
    QHash<QString, QString> m_customIds;
};

/// Treeland session policy: use the DDE keybinding service, which owns the
/// compositor-side shortcut manager in a normal DDE/Treeland login.
class TreelandShortcutBackend final : public DdeShortcutBackend {
public:
    QString name() const override;
    bool available() const override;
};
