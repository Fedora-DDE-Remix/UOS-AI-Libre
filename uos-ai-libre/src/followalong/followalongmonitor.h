// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPoint>
#include <QString>

/// Platform selection monitor used by AI FollowAlong.
///
/// X11 reads the PRIMARY selection through QClipboard. Wayland/Treeland uses
/// wlr-data-control because an unfocused application cannot observe another
/// client's selection through QClipboard.
class FollowAlongMonitor : public QObject
{
    Q_OBJECT

public:
    explicit FollowAlongMonitor(QObject *parent = nullptr);
    ~FollowAlongMonitor() override = default;

    static FollowAlongMonitor *create(QObject *parent = nullptr);

    virtual bool start(QString *error) = 0;
    virtual void stop() = 0;

    /// Forget the last text so selecting the same content again can show the
    /// toolbar after it has been dismissed.
    virtual void resetSelectionState() {}

Q_SIGNALS:
    void selectionChanged(const QString &text, const QPoint &globalPos);
    void selectionCleared();
    void monitorError(const QString &message);
};
