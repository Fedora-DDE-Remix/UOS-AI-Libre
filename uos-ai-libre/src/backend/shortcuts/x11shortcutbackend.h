#pragma once

#include "shortcutbackend.h"

#include <QAbstractNativeEventFilter>
#include <QVector>

/// X11 global shortcut backend. Used only when the DDE keybinding service is
/// not present; on a normal DDE session the DDE backend is preferred because
/// it shares ownership with the compositor/desktop.
class X11ShortcutBackend final : public QAbstractNativeEventFilter,
                                 public ShortcutBackend {
public:
    ~X11ShortcutBackend() override;

    QString name() const override;
    bool available() const override;
    bool registerShortcut(const QString &id, const QString &sequence,
                          const QString &command, QString *error) override;
    bool unregisterShortcut(const QString &id, QString *error) override;

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

private:
    struct Binding {
        QString id;
        unsigned int keycode = 0;
        unsigned int modifiers = 0;
        QString command;
    };

    QVector<Binding> m_bindings;
    bool m_filterInstalled = false;
};
