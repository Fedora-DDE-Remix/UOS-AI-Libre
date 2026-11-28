// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aibar.h"

#include <dde-shell/xdgactivation.h>
#include <pluginfactory.h>

#include <QDBusConnection>
#include <QDBusInterface>

namespace uosailibre {

AiBar::AiBar(QObject *parent) : DAppletDock(parent) {}

void AiBar::onClickIcon()
{
    // Keep the original UOS AI activation path: request an XDG activation
    // token on Wayland/Treeland so the focused chat window is allowed to raise,
    // then call the compatibility D-Bus method implemented by this app.
    auto *activation = new ds::XdgActivation(this);
    connect(activation, &ds::XdgActivation::tokenReady, this,
            [activation](const QString &token) {
                QDBusInterface copilot(
                    QStringLiteral("com.deepin.copilot"),
                    QStringLiteral("/com/deepin/copilot"),
                    QStringLiteral("com.deepin.copilot"),
                    QDBusConnection::sessionBus());
                if (token.isEmpty()) {
                    copilot.call(QStringLiteral("launchChatPage"));
                } else {
                    copilot.call(QStringLiteral("launchChatPageWithToken"), 0,
                                 token);
                }
                activation->deleteLater();
            });
    activation->requestToken();
}

DockItemInfo AiBar::dockItemInfo()
{
    DockItemInfo info;
    info.name = QStringLiteral("uos-ai");
    info.displayName = tr("UOS AI Libre");
    info.itemKey = QStringLiteral("uos-ai");
    info.settingKey = QStringLiteral("uos-ai");
    info.visible = visible();
    // The optional control-center icon uses a DCI. Keep the field empty
    // rather than pointing at UOS AI's old logo.
    info.dccIcon = QString();
    return info;
}

D_APPLET_CLASS(AiBar)

} // namespace uosailibre

#include "aibar.moc"
