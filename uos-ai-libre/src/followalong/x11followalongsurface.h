// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongview.h"

class X11FollowAlongSurface final : public FollowAlongSurface
{
    Q_OBJECT

public:
    X11FollowAlongSurface(QQmlEngine *engine, FollowAlongEventBus *bus,
                          QObject *parent = nullptr);
    ~X11FollowAlongSurface() override;

    FollowAlongCapabilities capabilities() const override;
    PresentationCommandResult present(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content) override;
    PresentationCommandResult update(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content) override;
    void dismiss() override;
    void requestTextInputFocus() override;
    QRect geometry() const override;
    bool isFocused() const override;

protected:
    void windowExposed() override;

private:
    PresentationCommandResult showSurface(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content, bool updateOnly);
    void applyGeometry();
    void rememberPreviousFocus();
    void restorePreviousFocusIfNeeded();
    void emitPresented(quint64 requestId, bool roleConfirmed);
    void wireSizeChanges();

    quint64 m_mappedRequestId = 0;
    unsigned long m_previousFocus = 0;
    bool m_hasPreviousFocus = false;
    bool m_sizeWired = false;
};
