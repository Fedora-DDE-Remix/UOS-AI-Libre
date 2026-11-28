// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "followalongtypes.h"

#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QSize>

class FollowAlongEventBus;
class QQmlComponent;
class QQmlEngine;
class QQuickWindow;
class QScreen;

/// Presentation/execution contract for the FollowAlong surface.
///
/// The controller sends a semantic FollowAlongPresentation. A backend
/// translates it into X11 window flags or a Treeland layer-shell surface and
/// reports what actually happened through PresentationEvents. A backend that
/// cannot guarantee an overlay/non-focus surface must fail; it must never
/// silently show a normal window.
class FollowAlongSurface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int maxContentWidth READ maxContentWidth NOTIFY maxContentWidthChanged)

public:
    FollowAlongSurface(QQmlEngine *engine, FollowAlongEventBus *bus,
                       QObject *parent = nullptr);
    ~FollowAlongSurface() override;

    static FollowAlongSurface *create(QQmlEngine *engine,
                                      FollowAlongEventBus *bus,
                                      QObject *parent = nullptr);

    virtual FollowAlongCapabilities capabilities() const = 0;
    virtual PresentationCommandResult present(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content) = 0;
    virtual PresentationCommandResult update(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content) = 0;
    virtual void dismiss() = 0;
    virtual void requestTextInputFocus() = 0;
    virtual QRect geometry() const = 0;
    virtual bool isFocused() const = 0;

    bool visible() const;
    bool menuOpen() const;
    int maxContentWidth() const;
    QString backendName() const;
    virtual bool usesInlineMenu() const { return false; }
    virtual void openMoreMenu();
    void closeMoreMenu();

Q_SIGNALS:
    void maxContentWidthChanged();
    void actionTriggered(int type, bool custom, const QString &text);
    void askRequested(const QString &question, const QString &text);
    void expandRequested();
    void dismissRequested();
    void settingsRequested();
    void moreMenuRequested();

protected:
    QQuickWindow *createWindow(const QString &componentName);
    void applyContent(const FollowAlongContent &content);
    void wireWindow(QQuickWindow *window);
    void releaseSwapchain();
    void destroyWindow();
    void postPresentation(PresentationEventType type, quint64 requestId,
                          const FollowAlongPresentationState &state,
                          const QString &error = QString());
    void noteOwnSurfaceInteraction();

    QScreen *screenForAnchor(const QPoint &anchor) const;
    QRect availableRect(const QPoint &anchor) const;
    QSize clampedSize(const QPoint &anchor, const QSize &size) const;
    QPoint positionForSize(const QPoint &anchor, const QSize &size) const;

    bool eventFilter(QObject *watched, QEvent *event) override;
    virtual void windowExposed() {}
    virtual void windowSurfaceCreated() {}

    QQmlEngine *m_engine = nullptr;
    FollowAlongEventBus *m_bus = nullptr;
    QQmlComponent *m_component = nullptr;
    QPointer<QQuickWindow> m_window;
    FollowAlongContent m_content;
    QPoint m_anchor;
    QRect m_lastGeometry;
    QString m_backendName;
    qint64 m_lastLocalPressMs = 0;
    qint64 m_lastOwnInteractionMs = 0;
    quint64 m_localPressSerial = 0;
    quint64 m_lastRequestId = 0;
    quint64 m_outsidePressSequence = 0;
    bool m_hasAnchor = false;
    bool m_swapchainReleased = false;
    bool m_exposeSeen = false;
    bool m_pointerInside = false;

private slots:
    void onWindowActionTriggered(int type, bool custom);
    void onWindowAskRequested(const QString &question);

private:
    void connectBus();
    void onSelectionEvent(const SelectionEvent &event);
    void scheduleOutsideCheck(const SelectionEvent &event);
    void emitFocusChanged();

    quint64 m_pendingOutsideSequence = 0;
};
