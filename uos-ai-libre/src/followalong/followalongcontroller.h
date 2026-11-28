// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongtypes.h"

#include <QObject>
#include <QPoint>
#include <QString>
#include <QVariantList>

class FollowAlongEventBus;
class FollowAlongSurface;
class QTimer;

/// Pure FollowAlong state machine.
///
/// It consumes SelectionEvents and PresentationEvents, sends semantic
/// presentation commands, and never includes platform headers, screens,
/// cursors, windows or protocol names. Success is decided by observed
/// RoleConfirmed/Mapped/FocusChanged events, not by present() returning.
class FollowAlongController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(bool expanded READ expanded NOTIFY expandedChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectedTextChanged)

public:
    FollowAlongController(FollowAlongEventBus *bus,
                          FollowAlongSurface *surface,
                          QObject *parent = nullptr);
    ~FollowAlongController() override;

    void setEnabled(bool enabled);
    bool enabled() const;

    void setActions(const QVariantList &actions,
                    const QVariantList &moreActions);
    void preview(const QString &text, const QPoint &anchor, bool expanded);
    void expand();
    void dismiss();

    bool visible() const;
    bool expanded() const;
    QString selectedText() const;

Q_SIGNALS:
    void visibleChanged();
    void expandedChanged();
    void selectedTextChanged();
    void selectionResetRequested();
    void presentedIcon();
    void presentationFailed(const QString &reason);

private:
    enum class State {
        Idle,
        Armed,
        Captured,
        PresentingIcon,
        IconShown,
        PresentingExpanded,
        Expanded,
    };

    void onSelectionEvent(const SelectionEvent &event);
    void onPresentationEvent(const PresentationEvent &event);
    void presentIcon();
    void presentExpanded();
    void maybeFinishPresentation();
    void failPresentation(const QString &reason);
    void clearSelection();
    void setState(State state);
    void postContent();
    FollowAlongPresentation basePresentation(bool expanded) const;

    FollowAlongEventBus *m_bus = nullptr;
    FollowAlongSurface *m_surface = nullptr;
    QTimer *m_timeout = nullptr;
    QVariantList m_actions;
    QVariantList m_moreActions;
    QString m_capturedText;
    QString m_lastError;
    QPoint m_anchor;
    quint64 m_nextRequestId = 0;
    quint64 m_pendingRequestId = 0;
    State m_state = State::Idle;
    bool m_enabled = true;
    bool m_hasAnchor = false;
    bool m_accepted = false;
    bool m_roleConfirmed = false;
    bool m_mapped = false;
    bool m_geometryReceived = false;
    bool m_focusOk = false;
};
