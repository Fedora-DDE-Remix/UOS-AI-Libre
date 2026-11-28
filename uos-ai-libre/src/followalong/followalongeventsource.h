// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongtypes.h"

#include <QObject>

class FollowAlongEventBus;

/// Platform selection event source.
///
/// Implementations own all raw protocol handling (XRecord, wlr-data-control,
/// treeland-active-notify) and post normalised SelectionEvents into the bus.
/// They never call the controller or create windows.
class FollowAlongEventSource : public QObject
{
    Q_OBJECT

public:
    explicit FollowAlongEventSource(FollowAlongEventBus *bus,
                                    QObject *parent = nullptr);
    ~FollowAlongEventSource() override = default;

    static FollowAlongEventSource *create(FollowAlongEventBus *bus,
                                          QObject *parent = nullptr);

    virtual bool start(QString *error) = 0;
    virtual void stop() = 0;
    virtual void resetSelectionState() = 0;
    virtual FollowAlongCapabilities capabilities() const = 0;

Q_SIGNALS:
    void monitorError(const QString &message);

protected:
    void post(SelectionEvent event);
    FollowAlongEventBus *m_bus = nullptr;
};
