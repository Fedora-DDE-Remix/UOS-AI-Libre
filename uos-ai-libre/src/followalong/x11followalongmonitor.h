// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongmonitor.h"

#include <QPointer>
#include <QString>

class QClipboard;
class QTimer;
class X11RecordWorker;

/// X11 PRIMARY-selection monitor. The selection is global on X11, so Qt can
/// observe it even though this application is not focused. XRecord is used to
/// require a real mouse press/release, exactly like UOS AI's FollowAlong.
class X11FollowAlongMonitor final : public FollowAlongMonitor
{
    Q_OBJECT

public:
    explicit X11FollowAlongMonitor(QObject *parent = nullptr);
    ~X11FollowAlongMonitor() override;

    bool start(QString *error) override;
    void stop() override;
    void resetSelectionState() override;

private:
    void onSelectionChanged();
    void onMousePressed(const QPoint &pos);
    void onMouseReleased(const QPoint &pos);
    void readSelection();
    void useSelectionFallback();

    QPointer<QClipboard> m_clipboard;
    QTimer *m_debounce = nullptr;
    QString m_lastText;
    QString m_pendingText;
    QPoint m_pressPos;
    QPoint m_releasePos;
    X11RecordWorker *m_record = nullptr;
    bool m_pressed = false;
    bool m_released = false;
    bool m_moved = false;
    bool m_recordAvailable = false;
};
