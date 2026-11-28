// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongeventsource.h"

#include <QPoint>
#include <QPointer>
#include <QString>

class QClipboard;
class QTimer;
class X11FollowAlongRecordWorker;

/// X11 PRIMARY-selection source.
///
/// QClipboard can observe the global PRIMARY selection on X11. XRecord gates
/// the event on a real left-button drag so ordinary clicks and toolbar clicks
/// never look like a new selection.
class X11FollowAlongEventSource final : public FollowAlongEventSource
{
    Q_OBJECT

public:
    explicit X11FollowAlongEventSource(FollowAlongEventBus *bus,
                                       QObject *parent = nullptr);
    ~X11FollowAlongEventSource() override;

    bool start(QString *error) override;
    void stop() override;
    void resetSelectionState() override;
    FollowAlongCapabilities capabilities() const override;

private:
    void onSelectionChanged();
    void onMousePressed(const QPoint &pos, bool left);
    void onMouseReleased(const QPoint &pos, bool left);
    void readSelection();
    void useSelectionFallback();

    QPointer<QClipboard> m_clipboard;
    QTimer *m_debounce = nullptr;
    QString m_lastText;
    QString m_pendingText;
    QPoint m_pressPos;
    QPoint m_releasePos;
    X11FollowAlongRecordWorker *m_record = nullptr;
    bool m_pressed = false;
    bool m_released = false;
    bool m_moved = false;
    bool m_recordAvailable = false;
};
