// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVariantList>

class QQmlComponent;
class QQmlEngine;
class QQuickWindow;
class QScreen;

/// Owns the small AI FollowAlong toolbar surface.
class FollowAlongWindowHost final : public QObject
{
    Q_OBJECT

public:
    explicit FollowAlongWindowHost(QQmlEngine *engine,
                                   QObject *parent = nullptr);
    ~FollowAlongWindowHost() override;

    void show(const QString &text, const QPoint &globalPos,
              const QVariantList &actions, bool expanded = false);
    void hide();
    bool visible() const;
    QRect geometry() const;
    bool cursorPositionAvailable() const;
    /// Global pointer position on Wayland through XWayland, when available.
    bool hasGlobalCursorPosition() const;
    QPoint globalCursorPosition() const;
    void setSize(const QSize &size);
    void setExpanded(bool expanded);
    void activateForInput();
    void openMoreMenu();
    int maxToolbarWidth() const;
    qint64 lastLocalPressMs() const;
    quint64 localPressSerial() const;
    quint64 generation() const;

Q_SIGNALS:
    void actionTriggered(int type, bool custom, const QString &text);
    void dismissed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onActionTriggered(int type, bool custom);
    void onDismissed();
    void onExpandedChanged();

private:
    QQmlComponent *component();
    void ensureWindow();
    void positionX11();
    void positionX11(const QSize &size);
    QPoint positionForSize(const QSize &size) const;
    QSize clampedSize(const QSize &size) const;
    QRect availableRect() const;
    QScreen *screenForAnchor() const;
    void updateWaylandPosition();
    void releaseSwapchain();
    void destroyWindow();

    QQmlEngine *m_engine = nullptr;
    QQmlComponent *m_component = nullptr;
    QPointer<QQuickWindow> m_window;
    QObject *m_overlay = nullptr;
    QPoint m_anchor;
    QPoint m_lastPosition;
    QString m_text;
    bool m_hasLastPosition = false;
    bool m_exactAnchor = false;
    bool m_showing = false;
    bool m_rebuildPending = false;
    bool m_forceExplicitAnchor = false;
    bool m_swapchainReleased = false;
    quint64 m_generation = 0;
    qint64 m_lastLocalPressMs = 0;
    quint64 m_localPressSerial = 0;
};
